#define WINDOW_WIDTH 1000
#define WINDOW_HEIGHT 500

#define NUM_SPHERES 2

#define MAX_SAMPLES 50
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

// =================================== Material interface ====================================

interface iMaterial
{

};

// ======================================= HitRecord =========================================

class HitRecord
{
	bool lambertian;
	Lambertian mat_lambert;

	float3 p;git 
	float3 normal;
	float t;
	bool hit;
};

// ====================================== Materials ===========================================

struct MaterialRay
{
	Ray r;
	float3 attenuation;
	bool scattered;
};

class Lambertian : iMaterial
{
	float3 albedo;

	MaterialRay scatter(Ray r, HitRecord hit_rec)
	{
		// Create MaterialRay
		MaterialRay scattered_ray;

		// Setup scattered ray
		float3 target = hit_rec.p + hit_rec.normal + random_in_unit_sphere();
		Ray ray;
		r.origin = hit_rec.p;
		r.direction = target;
		scattered_ray.r = ray;

		// Set attenuation
		attenuation = albedo;

		scattered_ray.scattered = true;

		// Return scattered ray data
		return scattered_ray;
	}
};

class Metal : iMaterial
{
	float3 albedo;

	MaterialRay scatter(Ray r, HitRecord hit_rec)
	{
		// Create MaterialRay
		MaterialRay scattered_ray;

		// Setup scattered ray
		float3 reflected = reflect(normalize(r.direction), hit_rec.normal);
		Ray ray;
		r.origin = hit_rec.p;
		r.direction = reflected;
		scattered_ray.r = ray;

		// Set attenuation
		attenuation = albedo;

		scattered_ray.scattered = (dot(ray.direction, rec_hit.normal) > 0);

		// Return scattered ray data
		return scattered_ray;
	}
};

// ============================== Hitable Geometry classes ===================================

class HitableSphere
{
	Lambertian mat;

	float3 center;
	float radius;
	HitRecord hit(Ray r, float t_min, float t_max)
	{
		// Initialize hit record
		HitRecord hit_rec;
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

// ================================ Hitable Manager class ============================================

class HitableManager
{
	HitRecord hit(Ray r, float t_min, float t_max)
	{
		// Hitable geometry
		HitableSphere spheres[NUM_SPHERES];
		Lambertian lam;

		lam.albedo = float3(0.8, 0.3, 0.3);
		spheres[0].center = float3(0.0, 0.0, -1.0);
		spheres[0].radius = 0.5;
		spheres[0].mat = lam;

		lam.albedo = float3(0.8, 0.6, 0.2);
		spheres[1].center = float3(0.0, -100.5, -10.0);
		spheres[1].radius = 100.0;
		spheres[1].mat = lam;

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

HitableManager hitable_manager;

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

// ================================= Color function =========================================

struct ColorData
{
	float3 color;
	Ray rebounce_ray;
	bool rebounce;
};

ColorData color(Ray r, float2 randomizer)
{
	// Create color data to return
	ColorData color_data;
	color_data.rebounce = false;
	color_data.color = float3(0.0, 0.0, 0.0);

	// Create Hit record to record hit(s)
	HitRecord rec;
	rec.hit = false;

	// Run hit manager
	rec = (hitable_manager.hit(r, 0.001, 3E+38));

	// Check if hit was found
	if (rec.hit)
	{
		// Create new ray for bounce
		float3 target = rec.p + rec.normal + random_in_unit_sphere(randomizer);
		Ray rebounce_ray;
		rebounce_ray.origin = rec.p;
		rebounce_ray.direction = target;

		// Bounce and return final color
		color_data.rebounce = true;
		color_data.rebounce_ray = rebounce_ray;
		color_data.color = float3(0.1, 0.1, 0.5);
		return color_data;
	}
	else
	{
		// Ray didn't hit anything anymore
		float3 n_dir = normalize(r.direction);
		float t = 0.5 * (n_dir.y + 1.0);

		// No bounce and return final color
		color_data.rebounce = false;
		color_data.color = (1.0 - t) * float3(1.0, 1.0, 1.0) + t * float3(0.5, 0.7, 1.0);
		return color_data;
	}
}

// =================================== Handle bounces ============================================

float4 handleBounces(ColorData color_data, float2 randomizer, float4 final_color)
{
	for (int i = 0; color_data.rebounce; i++)
	{
		randomizer += float2(812.0, 122.0);
		color_data = color(color_data.rebounce_ray, randomizer);
	}

	final_color = float4(color_data.color.xyz, 1.0);

	// i is the amount of bounces that happened
	// Recursive calculations may happen here
	if (i > 0)
	{
		final_color = pow(0.5, i);
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

	float2 randomizer = float2(rand(uv), rand(uv));
	for (int s = 0; s < MAX_SAMPLES; ++s)
	{
		// Get slightly offset uv's
		float u = float(pixel_coords.x + (SAMPLE_FACTOR - (rand(randomizer) * MAX_SAMPLE_OFFSET))) / float(WINDOW_WIDTH);
		float v = float(pixel_coords.y + (SAMPLE_FACTOR - (rand(randomizer) * MAX_SAMPLE_OFFSET))) / float(WINDOW_HEIGHT);

		// Create Ray
		Ray r = cam.get_ray(u, v);

		// Get final color
		ColorData color_data = color(r, randomizer);
		float4 bounced_color = handleBounces(color_data, randomizer, final_color);

		//final_color += float4(color_data.color, 1.0);
		final_color += bounced_color;

		// Update randomizer		
		randomizer += (float2(final_color.xy) * float2(final_color.x - final_color.y, final_color.y - final_color.x)) * 500.f;
	}
	final_color /= float4(MAX_SAMPLES, MAX_SAMPLES, MAX_SAMPLES, 1.0);
	final_color = float4(sqrt(final_color.x), sqrt(final_color.y), sqrt(final_color.z), 1.0);

	// render final color
	return final_color;
}