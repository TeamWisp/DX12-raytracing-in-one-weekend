#define WINDOW_WIDTH 1000
#define WINDOW_HEIGHT 500

#define NUM_SPHERES 4

#define MAX_SAMPLES 10
#define MAX_SAMPLE_OFFSET 1.0
#define SAMPLE_FACTOR (MAX_SAMPLE_OFFSET / 2.0)

// ================================== Shader Parameters ==========================================

struct VSOutput
{
	float2 pTexture    : P_TEXTURECOORD;
};

Texture2D t1 : register(t0);
SamplerState s1 : register(s0);

// ====================================== Random ===================================

float rand(float2 co)
{
	return frac(sin(dot(co.xy, float2(12.9898, 78.233))) * 43758.5453);
}

// ==================================== Ray class =========================================

class Ray
{
	float3 origin;
	float3 direction;
	float3 point_at(float t)
	{
		return origin + t * direction;
	}
};

// ================================== Camera class =========================================

class Camera
{
	float3 origin;
	float3 lower_left_corner;
	float3 horizontal;
	float3 vertical;

	void init()
	{
		origin = float3(0.0, 0.0, 0.0);
		lower_left_corner = float3(-2.0, -1.0, -1.0);
		horizontal = float3(4.0, 0.0, 0.0);
		vertical = float3(0.0, 2.0, 0.0);
	}

	Ray get_ray(float u, float v)
	{
		Ray r;
		r.origin = origin;
		r.direction = lower_left_corner + u * horizontal + v * vertical;
		return r;
	}
};

// ==================================== Material ==============================================

struct MaterialOptions
{
	// General variables
	float3 albedo;

	// MATERIAL TYPES
	// Lambertian
	bool lambert;
	// Metal
	bool metal;
};

MaterialOptions create_material_options()
{
	MaterialOptions options;
	options.albedo = float3(0.0, 0.0, 0.0);

	options.lambert = false;
	options.metal = false;
	return options;
}

struct MaterialHitRecord
{
	Ray scatter_ray;
	float3 attenuation;
	bool scattered;
};

// ================================= HitRecord class ==========================================

struct HitRecord
{
	MaterialOptions mat;
	float3 p;
	float3 normal;
	float t;
	bool hit;
};

// ============================== Hitable Geometry classes ===================================

class HitableSphere
{
	MaterialOptions mat;

	float3 center;
	float radius;
	HitRecord hit(Ray r, float t_min, float t_max)
	{
		// Initialize hit record
		HitRecord hit_rec;
		hit_rec.mat = mat;
		hit_rec.t = 0.0;
		hit_rec.p = float3(0.0, 0.0, 0.0);
		hit_rec.normal = float3(0.0, 1.0, 0.0);

		// Check if ray hits sphere
		float3 oc = r.origin - center;
		float a = dot(r.direction, r.direction);
		float b = 2.0 * dot(oc, r.direction);
		float c = dot(oc, oc) - radius * radius;
		float discriminant = b * b - 4 * a*c;
		if (discriminant > 0)
		{
			float temp = (-b - sqrt(b*b - a * c)) / a;
			if (temp < t_max && temp > t_min)
			{
				hit_rec.t = temp;
				hit_rec.p = r.point_at(hit_rec.t);
				hit_rec.normal = (hit_rec.p - center) / radius;
				hit_rec.hit = true;
				return hit_rec;
			}
			temp = (-b + sqrt(b*b - a * c)) / a;
			if (temp < t_max && temp > t_min)
			{
				hit_rec.t = temp;
				hit_rec.p = r.point_at(hit_rec.t);
				hit_rec.normal = (hit_rec.p - center) / radius;
				hit_rec.hit = true;
				return hit_rec;
			}
		}
		hit_rec.hit = false;
		return hit_rec;
	}
};

// ========================= Random unit in sphere function ===================================

float3 random_in_unit_sphere(float2 randomizer)
{
	float3 p;
	// Calculate random direction
	p = (2.0 *
		 float3(
		 rand(randomizer + float2(832.0, 0.0)),
		 rand(randomizer + float2(0.0, 652.0)),
		 rand(randomizer + float2(876.0, 324.0)))) -
		float3(1, 1, 1);

	return normalize(p);
}

// ================================== Material functions =============================================

MaterialHitRecord lambertian_scatter(Ray r, HitRecord hit_rec, MaterialOptions options, float2 randomizer)
{
	// Create MaterialHitRecord
	MaterialHitRecord mat_hit;

	// Setup scattered ray
	float3 target = hit_rec.p + hit_rec.normal + random_in_unit_sphere(randomizer);
	Ray ray;
	ray.origin = hit_rec.p;
	ray.direction = target;
	mat_hit.scatter_ray = ray;

	// Set attenuation
	mat_hit.attenuation = options.albedo;

	mat_hit.scattered = true;

	// Return scattered ray data
	return mat_hit;
}

MaterialHitRecord metal_scatter(Ray r, HitRecord hit_rec, MaterialOptions options, float2 randomizer)
{
	// Create MaterialHitRecord
	MaterialHitRecord mat_hit;

	// Setup scattered ray
	Ray ray;
	ray.origin = hit_rec.p;
	ray.direction = reflect(normalize(r.direction), hit_rec.normal);
	mat_hit.scatter_ray = ray;

	// Set attenuation
	mat_hit.attenuation = options.albedo;

	mat_hit.scattered = (dot(ray.direction, hit_rec.normal) > 0);

	// Return scattered ray data
	return mat_hit;
}

// ================================ Hitable Manager class ============================================

class HitableManager
{
	// Hitable geometry
	HitableSphere spheres[NUM_SPHERES];

	void init()
	{
		spheres[0].center = float3(0.0, 0.0, -1.0);
		spheres[0].radius = 0.5;
		spheres[0].mat = create_material_options();
		spheres[0].mat.lambert = true;
		spheres[0].mat.albedo = float3(0.8, 0.3, 0.3); // albedo

		spheres[3].center = float3(0.0, -100.5, -10.0);
		spheres[3].radius = 100.0;
		spheres[3].mat = create_material_options();
		spheres[3].mat.metal = true;
		spheres[3].mat.albedo = float3(0.8, 0.8, 0.0); // albedo

		spheres[2].center = float3(1.0, 0.0, -1.0);
		spheres[2].radius = 0.5;
		spheres[2].mat = create_material_options();
		spheres[2].mat.lambert = true;
		spheres[2].mat.albedo = float3(0.8, 0.6, 0.2); // albedo

		spheres[1].center = float3(-1.0, 0.0, -1.0);
		spheres[1].radius = 0.5;
		spheres[1].mat = create_material_options();
		spheres[1].mat.lambert = true;
		spheres[1].mat.albedo = float3(0.8, 0.8, 0.8); // albedo
	}

	HitRecord hit(Ray r, float t_min, float t_max)
	{
		// Check for hits
		HitRecord hit_rec;
		hit_rec.hit = false;
		float closest_so_far = t_max;
		// Check spheres
		for (int i = 0; i < NUM_SPHERES; ++i)
		{
			HitRecord sphere_hit = spheres[i].hit(r, t_min, closest_so_far);
			if (sphere_hit.hit)
			{
				closest_so_far = hit_rec.t;
				hit_rec = sphere_hit;
			}
		}
		return hit_rec;
	}
};

// ================================= Color function =========================================

struct ColorData
{
	MaterialHitRecord mat_hit;
};

ColorData color(HitableManager hit_manager, Ray r, float2 randomizer)
{
	// Create color data to return
	ColorData color_data;

	// Create Hit record to record hit(s)
	HitRecord rec;
	rec.hit = false;

	// Run hit manager
	rec = (hit_manager.hit(r, 0.001, 3E+38));

	// Check if hit was found
	if (rec.hit)
	{
		if (rec.mat.lambert)
		{
			color_data.mat_hit = lambertian_scatter(r, rec, rec.mat, randomizer);
		}
		else if (rec.mat.metal)
		{
			color_data.mat_hit = metal_scatter(r, rec, rec.mat, randomizer);
			
		}

		return color_data;
	}
	else
	{
		// Ray didn't hit anything anymore
		float3 n_dir = normalize(r.direction);
		float t = 0.5 * (n_dir.y + 1.0);

		color_data.mat_hit.scatter_ray = r;
		color_data.mat_hit.attenuation = (1.0 - t) * float3(1.0, 1.0, 1.0) + t * float3(0.5, 0.7, 1.0);
		color_data.mat_hit.scattered = false;

		return color_data;
	}
}

// =================================== Handle bounces ============================================

float4 handleBounces(HitableManager hit_manager, ColorData color_data, float2 randomizer, float4 final_color)
{
	final_color = float4(color_data.mat_hit.attenuation.xyz, 1.0);

	for (int i = 0; i < 50; i++)
	{
		randomizer += float2(812.0, 122.0);
		color_data = color(hit_manager, color_data.mat_hit.scatter_ray, randomizer);
		if (color_data.mat_hit.scattered)
		{
			final_color *= float4(color_data.mat_hit.attenuation.xyz, 1.0);
		}
		else
		{
			break;
		}
	}

	return final_color;
}

// =================================== MAIN ============================================

float4 main(VSOutput IN) : SV_Target
{
	// Setup general variables
	float2 dims = float2(
		WINDOW_WIDTH,
		WINDOW_HEIGHT
	);

	float2 uv = float2(IN.pTexture.x, IN.pTexture.y);
	float2 pixel_position = float2(
		uv * dims
	);

	float4 final_color = float4(0.0, 0.0, 0.0, 1.0);
	float2 pixel_coords = float2(pixel_position.xy);

	// Create camera
	Camera cam;
	cam.init();

	// Create hitable manager
	HitableManager hitable_manager;
	hitable_manager.init();

	float2 randomizer = float2(rand(uv), rand(uv));
	for (int s = 0; s < MAX_SAMPLES; ++s)
	{
		// Get slightly offset uv's
		float u = float(pixel_coords.x + (SAMPLE_FACTOR - (rand(randomizer) * MAX_SAMPLE_OFFSET))) / float(WINDOW_WIDTH);
		float v = float(pixel_coords.y + (SAMPLE_FACTOR - (rand(randomizer) * MAX_SAMPLE_OFFSET))) / float(WINDOW_HEIGHT);

		// Create Ray
		Ray r = cam.get_ray(u, v);

		// Get final color
		ColorData color_data = color(hitable_manager, r, randomizer);
		float4 bounced_color = handleBounces(hitable_manager, color_data, randomizer, final_color);

		final_color += bounced_color;

		// Update randomizer		
		randomizer += (float2(final_color.xy) * float2(final_color.x - final_color.y, final_color.y - final_color.x)) * 500.f;
	}
	final_color /= float4(MAX_SAMPLES, MAX_SAMPLES, MAX_SAMPLES, 1.0);
	final_color = float4(sqrt(final_color.x), sqrt(final_color.y), sqrt(final_color.z), 1.0);

	// render final color
	return final_color;
}
