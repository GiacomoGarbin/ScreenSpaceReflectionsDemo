Texture2D<float4>   g_lit_scene              : register(t0);
Texture2D<float>    g_depth_buffer_hierarchy : register(t1);
Texture2D<float4>   g_normal                 : register(t2);

StructuredBuffer<uint> g_sobol_buffer             : register(t5);
StructuredBuffer<uint> g_ranking_tile_buffer      : register(t6);
StructuredBuffer<uint> g_scrambling_tile_buffer   : register(t7);

RWTexture2D<float4> gOutputTexture           : register(u0);

#define FFX_SSSR_FLOAT_MAX                 3.402823466e+38
#define M_PI                               3.14159265358979323846
#define GOLDEN_RATIO                       1.61803398875f

cbuffer ConstantBuffer : register(b0)
{
    float4x4 g_view;
    float4x4 g_inv_view;
    float4x4 g_proj;
    float4x4 g_inv_proj;
    float4x4 g_inv_view_proj;

    uint g_frame_index;

    float3 padding;
};

// Transforms origin to uv space
// Mat must be able to transform origin from its current space into clip space.
float3 ProjectPosition(float3 origin, float4x4 mat)
{
    float4 projected = mul(mat, float4(origin, 1));
    projected.xyz /= projected.w;
    projected.xy = 0.5 * projected.xy + 0.5;
    projected.y = (1 - projected.y);
    return projected.xyz;
}

// Origin and direction must be in the same space and mat must be able to transform from that space into clip space.
float3 ProjectDirection(float3 origin, float3 direction, float3 screen_space_origin, float4x4 mat)
{
    float3 offsetted = ProjectPosition(origin + direction, mat);
    return offsetted - screen_space_origin;
}

float3x3 CreateTBN(float3 N)
{
    float3 U;
    
    if (abs(N.z) > 0.0)
    {
        float k = sqrt(N.y * N.y + N.z * N.z);
        U.x = 0.0; U.y = -N.z / k; U.z = N.y / k;
    }
    else
    {
        float k = sqrt(N.x * N.x + N.y * N.y);
        U.x = N.y / k; U.y = -N.x / k; U.z = 0.0;
    }

    float3x3 TBN;
    TBN[0] = U;
    TBN[1] = cross(N, U);
    TBN[2] = N;
    return transpose(TBN);
}

// Blue Noise Sampler by Eric Heitz. Returns a value in the range [0, 1].
float SampleRandomNumber(uint pixel_i, uint pixel_j, uint sample_index, uint sample_dimension)
{
    // Wrap arguments
    pixel_i = pixel_i & 127u;
    pixel_j = pixel_j & 127u;
    sample_index = sample_index & 255u;
    sample_dimension = sample_dimension & 255u;

#ifndef SPP
#define SPP 1
#endif

#if SPP == 1
    const uint ranked_sample_index = sample_index ^ 0;
#else
    // xor index based on optimized ranking
    const uint ranked_sample_index = sample_index ^ g_ranking_tile_buffer[sample_dimension + (pixel_i + pixel_j * 128u) * 8u];
#endif

    // Fetch value in sequence
    uint value = g_sobol_buffer[sample_dimension + ranked_sample_index * 256u];

    // If the dimension is optimized, xor sequence value based on optimized scrambling
    value = value ^ g_scrambling_tile_buffer[(sample_dimension % 8u) + (pixel_i + pixel_j * 128u) * 8u];

    // Convert to float and return
    return (value + 0.5f) / 256.0f;
}

float2 SampleRandomVector2D(uint2 pixel)
{
    float2 u = float2
    (
        fmod(SampleRandomNumber(pixel.x, pixel.y, 0, 0u) + (g_frame_index & 0xFFu) * GOLDEN_RATIO, 1.0f),
        fmod(SampleRandomNumber(pixel.x, pixel.y, 0, 1u) + (g_frame_index & 0xFFu) * GOLDEN_RATIO, 1.0f)
    );
    return u;
}

float3 SampleGGXVNDF(float3 Ve, float alpha_x, float alpha_y, float U1, float U2)
{
    // Section 3.2: transforming the view direction to the hemisphere configuration
    float3 Vh = normalize(float3(alpha_x * Ve.x, alpha_y * Ve.y, Ve.z));
    // Section 4.1: orthonormal basis (with special case if cross product is zero)
    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    float3 T1 = lensq > 0 ? float3(-Vh.y, Vh.x, 0) * rsqrt(lensq) : float3(1, 0, 0);
    float3 T2 = cross(Vh, T1);
    // Section 4.2: parameterization of the projected area
    float r = sqrt(U1);
    float phi = 2.0 * M_PI * U2;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;
    // Section 4.3: reprojection onto hemisphere
    float3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;
    // Section 3.4: transforming the normal back to the ellipsoid configuration
    float3 Ne = normalize(float3(alpha_x * Nh.x, alpha_y * Nh.y, max(0.0, Nh.z)));
    return Ne;
}

float3 Sample_GGX_VNDF_Ellipsoid(float3 Ve, float alpha_x, float alpha_y, float U1, float U2)
{
    return SampleGGXVNDF(Ve, alpha_x, alpha_y, U1, U2);
}

float3 Sample_GGX_VNDF_Hemisphere(float3 Ve, float alpha, float U1, float U2)
{
    return Sample_GGX_VNDF_Ellipsoid(Ve, alpha, alpha, U1, U2);
}

float3 SampleReflectionVector(float3 view_direction, float3 normal, float roughness, int2 dispatch_thread_id)
{
    float3x3 tbn_transform = CreateTBN(normal);
    float3 view_direction_tbn = mul(-view_direction, tbn_transform);

    float2 u = SampleRandomVector2D(dispatch_thread_id);

    float3 sampled_normal_tbn = Sample_GGX_VNDF_Hemisphere(view_direction_tbn, roughness, u.x, u.y);
#ifdef PERFECT_REFLECTIONS
    //sampled_normal_tbn = float3(0, 0, 1); // Overwrite normal sample to produce perfect reflection.
#endif

    float3 reflected_direction_tbn = reflect(-view_direction_tbn, sampled_normal_tbn);

    // Transform reflected_direction back to the initial space.
    float3x3 inv_tbn_transform = transpose(tbn_transform);
    return mul(reflected_direction_tbn, inv_tbn_transform);
}

// Mat must be able to transform origin from texture space to a linear space.
float3 InvProjectPosition(float3 coord, float4x4 mat)
{
    coord.y = (1 - coord.y);
    coord.xy = 2 * coord.xy - 1;
    float4 projected = mul(mat, float4(coord, 1));
    projected.xyz /= projected.w;
    return projected.xyz;
}

float3 FFX_DNSR_Reflections_ScreenSpaceToViewSpace(float3 screen_uv_coord)
{
    return InvProjectPosition(screen_uv_coord, g_inv_proj);
}

float3 FFX_SSSR_ScreenSpaceToViewSpace(float3 screen_space_position)
{
    return InvProjectPosition(screen_space_position, g_inv_proj);
}

float3 ScreenSpaceToWorldSpace(float3 screen_space_position)
{
    return InvProjectPosition(screen_space_position, g_inv_view_proj);
}

float FFX_SSSR_LoadDepth(int2 pixel_coordinate, int mip)
{
    return g_depth_buffer_hierarchy.Load(int3(pixel_coordinate, mip));
}

bool IsMirrorReflection(float roughness)
{
    return roughness < 0.0001;
}

float3 FFX_SSSR_LoadNormal(int2 pixel_coordinate)
{
    //return 2 * g_normal.Load(int3(pixel_coordinate, 0)).xyz - 1;
    return g_normal.Load(int3(pixel_coordinate, 0)).xyz;

    //float3 view_space_normal  = g_normal.Load(int3(pixel_coordinate, 0)).xyz;
    //float3 world_space_normal = mul(g_inv_view, float4(normalize(view_space_normal), 0)).xyz;
    //return world_space_normal;
}

void FFX_SSSR_InitialAdvanceRay
(
    float3 origin,
    float3 direction,
    float3 inv_direction,
    float2 current_mip_resolution,
    float2 current_mip_resolution_inv,
    float2 floor_offset,
    float2 uv_offset,
    out float3 position,
    out float current_t
)
{
    float2 current_mip_position = current_mip_resolution * origin.xy;

    // Intersect ray with the half box that is pointing away from the ray origin.
    float2 xy_plane = floor(current_mip_position) + floor_offset;
    xy_plane = xy_plane * current_mip_resolution_inv + uv_offset;

    // o + d * t = p' => t = (p' - o) / d
    float2 t = (xy_plane - origin.xy) * inv_direction.xy;
    current_t = min(t.x, t.y);
    position = origin + current_t * direction;
}

bool FFX_SSSR_AdvanceRay
(
    float3 origin,
    float3 direction,
    float3 inv_direction,
    float2 current_mip_position,
    float2 current_mip_resolution_inv,
    float2 floor_offset,
    float2 uv_offset,
    float surface_z,
    inout float3 position,
    inout float current_t
)
{
    // Create boundary planes
    float2 xy_plane = floor(current_mip_position) + floor_offset;
    xy_plane = xy_plane * current_mip_resolution_inv + uv_offset;
    float3 boundary_planes = float3(xy_plane, surface_z);

    // Intersect ray with the half box that is pointing away from the ray origin.
    // o + d * t = p' => t = (p' - o) / d
    float3 t = (boundary_planes - origin) * inv_direction;

    // Prevent using z plane when shooting out of the depth buffer.
#ifdef FFX_SSSR_INVERTED_DEPTH_RANGE
    t.z = direction.z < 0 ? t.z : FFX_SSSR_FLOAT_MAX;
#else
    t.z = direction.z > 0 ? t.z : FFX_SSSR_FLOAT_MAX;
#endif

    // Choose nearest intersection with a boundary.
    float t_min = min(min(t.x, t.y), t.z);

#ifdef FFX_SSSR_INVERTED_DEPTH_RANGE
    // Larger z means closer to the camera.
    bool above_surface = surface_z < position.z;
#else
    // Smaller z means closer to the camera.
    bool above_surface = surface_z > position.z;
#endif

    // Decide whether we are able to advance the ray until we hit the xy boundaries or if we had to clamp it at the surface.
    bool skipped_tile = t_min != t.z && above_surface;

    // Make sure to only advance the ray if we're still above the surface.
    current_t = above_surface ? t_min : current_t;

    // Advance ray
    position = origin + current_t * direction;

    return skipped_tile;
}

float2 FFX_SSSR_GetMipResolution(float2 screen_dimensions, int mip_level)
{
    return screen_dimensions * pow(0.5, mip_level);
}

// Requires origin and direction of the ray to be in screen space [0, 1] x [0, 1]
float3 FFX_SSSR_HierarchicalRaymarch
(
    float3 origin,
    float3 direction,
    bool is_mirror,
    float2 screen_size,
    int most_detailed_mip,
    uint min_traversal_occupancy,
    uint max_traversal_intersections,
    out bool valid_hit
)
{
    const float3 inv_direction = direction != 0 ? 1.0 / direction : FFX_SSSR_FLOAT_MAX;

    // Start on mip with highest detail.
    int current_mip = most_detailed_mip;

    // Could recompute these every iteration, but it's faster to hoist them out and update them.
    float2 current_mip_resolution = FFX_SSSR_GetMipResolution(screen_size, current_mip);
    float2 current_mip_resolution_inv = rcp(current_mip_resolution);

    // Offset to the bounding boxes uv space to intersect the ray with the center of the next pixel.
    // This means we ever so slightly over shoot into the next region. 
    float2 uv_offset = 0.005 * exp2(most_detailed_mip) / screen_size;
    uv_offset = direction.xy < 0 ? -uv_offset : uv_offset;

    // Offset applied depending on current mip resolution to move the boundary to the left/right upper/lower border depending on ray direction.
    float2 floor_offset = direction.xy < 0 ? 0 : 1;

    // Initially advance ray to avoid immediate self intersections.
    float current_t;
    float3 position;
    FFX_SSSR_InitialAdvanceRay(origin, direction, inv_direction, current_mip_resolution, current_mip_resolution_inv, floor_offset, uv_offset, position, current_t);

    bool exit_due_to_low_occupancy = false;
    int i = 0;
    while (i < max_traversal_intersections && current_mip >= most_detailed_mip && !exit_due_to_low_occupancy)
    {
        float2 current_mip_position = current_mip_resolution * position.xy;
        float surface_z = FFX_SSSR_LoadDepth(current_mip_position, current_mip);
        bool skipped_tile = FFX_SSSR_AdvanceRay(origin, direction, inv_direction, current_mip_position, current_mip_resolution_inv, floor_offset, uv_offset, surface_z, position, current_t);
        current_mip += skipped_tile ? 1 : -1;
        current_mip_resolution *= skipped_tile ? 0.5 : 2;
        current_mip_resolution_inv *= skipped_tile ? 2 : 0.5;
        ++i;

        // exit_due_to_low_occupancy = !is_mirror; // && WaveActiveCountBits(true) <= min_traversal_occupancy;
    }

    valid_hit = (i <= max_traversal_intersections);

    return position;
}

float FFX_SSSR_ValidateHit(float3 hit, float2 uv, float3 world_space_ray_direction, float2 screen_size, float depth_buffer_thickness)
{
    // Reject hits outside the view frustum
    if (any(hit.xy < 0) || any(hit.xy > 1))
    {
        return 0;
    }

    // Reject the hit if we didnt advance the ray significantly to avoid immediate self reflection
    float2 manhattan_dist = abs(hit.xy - uv);
    if (all(manhattan_dist < (2 / screen_size)))
    {
        return 0;
    }

    // Don't lookup radiance from the background.
    int2 texel_coords = int2(screen_size * hit.xy);
    float surface_z = FFX_SSSR_LoadDepth(texel_coords / 2, 1);
#ifdef FFX_SSSR_INVERTED_DEPTH_RANGE
    if (surface_z == 0.0)
#else
    if (surface_z == 1.0)
#endif
    {
        return 0;
    }

    // We check if we hit the surface from the back, these should be rejected.
    float3 hit_normal = FFX_SSSR_LoadNormal(texel_coords);
    if (dot(hit_normal, world_space_ray_direction) > 0)
    {
        return 0;
    }

    float3 view_space_surface = FFX_SSSR_ScreenSpaceToViewSpace(float3(hit.xy, surface_z));
    float3 view_space_hit = FFX_SSSR_ScreenSpaceToViewSpace(hit);
    float distance = length(view_space_surface - view_space_hit);

    // Fade out hits near the screen borders
    float2 fov = 0.05 * float2(screen_size.y / screen_size.x, 1);
    float2 border = smoothstep(0, fov, hit.xy) * (1 - smoothstep(1 - fov, 1, hit.xy));
    float vignette = border.x * border.y;

    // We accept all hits that are within a reasonable minimum distance below the surface.
    // Add constant in linear space to avoid growing of the reflections toward the reflected objects.
    float confidence = 1 - smoothstep(0, depth_buffer_thickness, distance);
    confidence *= confidence;

    return vignette * confidence;
}

[numthreads(8, 8, 1)]
void main(uint3 GroupThreadID : SV_GroupThreadID, uint3 DispatchThreadID : SV_DispatchThreadID)
{
    uint g_most_detailed_mip = 1;
    uint g_min_traversal_occupancy = 4;
    uint g_max_traversal_intersections = 128;
    float g_depth_buffer_thickness = 0.015f;

    uint2 screen_size;
    gOutputTexture.GetDimensions(screen_size.x, screen_size.y);

    uint2 coords = DispatchThreadID.xy;
    float2 uv = (coords + 0.5) / screen_size;

    //float3 view_space_surface_normal = g_normal.Load(int3(coords, 0)).xyz;
    //float3 world_space_normal = mul(g_inv_view, float4(normalize(view_space_surface_normal), 0)).xyz;
    float3 world_space_normal = g_normal.Load(int3(coords, 0)).xyz;
    float3 view_space_surface_normal = mul(g_view, float4(normalize(world_space_normal), 0)).xyz;

    float roughness = 0; // g_roughness.Load(int3(coords, 0));
    bool is_mirror = IsMirrorReflection(roughness);

    int most_detailed_mip = is_mirror ? 0 : g_most_detailed_mip;
    float2 mip_resolution = FFX_SSSR_GetMipResolution(screen_size, most_detailed_mip);
    float z = FFX_SSSR_LoadDepth(uv * mip_resolution, most_detailed_mip);

    float3 screen_uv_space_ray_origin = float3(uv, z);
    float3 view_space_ray = FFX_DNSR_Reflections_ScreenSpaceToViewSpace(screen_uv_space_ray_origin);
    float3 view_space_ray_direction = normalize(view_space_ray);

    float3 view_space_reflected_direction = SampleReflectionVector(view_space_ray_direction, view_space_surface_normal, roughness, coords);
    float3 screen_space_ray_direction = ProjectDirection(view_space_ray, view_space_reflected_direction, screen_uv_space_ray_origin, g_proj);

    bool valid_hit = false;
    float3 hit = FFX_SSSR_HierarchicalRaymarch(screen_uv_space_ray_origin, screen_space_ray_direction, is_mirror, screen_size, most_detailed_mip, g_min_traversal_occupancy, g_max_traversal_intersections, valid_hit);

    float3 world_space_origin = ScreenSpaceToWorldSpace(screen_uv_space_ray_origin);
    float3 world_space_hit = ScreenSpaceToWorldSpace(hit);
    float3 world_space_ray = world_space_hit - world_space_origin.xyz;
    float confidence = valid_hit ? FFX_SSSR_ValidateHit(hit, uv, world_space_ray, screen_size, g_depth_buffer_thickness) : 0;

    float3 reflection_radiance = 0;
    if (confidence > 0)
    {
        // Found an intersection with the depth buffer -> We can lookup the color from lit scene.
        reflection_radiance = g_lit_scene.Load(int3(screen_size * hit.xy, 0)).xyz;
    }

    gOutputTexture[DispatchThreadID.xy] = float4(hit.xy, 0, confidence);

    //float2 u = SampleRandomVector2D(DispatchThreadID.xy);
    //gOutputTexture[DispatchThreadID.xy] = float4(u, 0, 1);

    //uint value = g_sobol_buffer[0];
    //gOutputTexture[DispatchThreadID.xy] = float4(value, 0, 0, 1);

    //gOutputTexture[DispatchThreadID.xy] = float4(FFX_SSSR_LoadNormal(coords), 1);
    //gOutputTexture[DispatchThreadID.xy] = float4(z, z, z, 1);
    //gOutputTexture[DispatchThreadID.xy] = float4(screen_uv_space_ray_origin + screen_space_ray_direction, 1);
    //gOutputTexture[DispatchThreadID.xy] = float4(reflection_radiance, 1);
    //gOutputTexture[DispatchThreadID.xy] = float4(g_lit_scene.Load(int3(coords, 0)).xyz, 1);
}