#define WINDOW_WIDTH 1000
#define WINDOW_HEIGHT 500

struct VSOutput
{
	float2 pTexture    : P_TEXTURECOORD;
};

Texture2D t1 : register(t0);
SamplerState s1 : register(s0);

struct Ray
{
	float3 origin;
	float3 direction;
	float3 point_at(float t)
	{ 
		return origin + t*direction;
	}
};

// ============================================================================

struct HitRecord
{
	float3 p;
	float3 normal;
	float t;
	bool hit;
};

struct HitableSphere
{
	float3 center;
	float radius;
	HitRecord hit(Ray r, float t_min, float t_max)
	{
		HitRecord hit_rec;
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

#define NUM_SPHERES 2

struct HitableManager
{
	HitRecord hit(Ray r, float t_min, float t_max)
	{
		// Hitable geometry
		HitableSphere spheres[NUM_SPHERES];

		spheres[0].center = float3(0.0, 0.0, -1.0);
		spheres[0].radius = 0.5;

		spheres[1].center = float3(0.0, -100.5, -10.0);
		spheres[1].radius = 100.0;

		// Check for hits
		HitRecord hit_rec;
		hit_rec.hit = false;
		bool hit_anything = false;
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

// ============================================================================

float3 color(Ray r)
{
	HitRecord rec;
	rec.hit = false;
	HitableSphere sphere;
	sphere.center = float3(0,0,-1);
	sphere.radius = 0.5;
	rec = (hitable_manager.hit(r, 0.0, 3E+38));
	if (rec.hit)
	{
		return 0.5*float3(rec.normal.x+1, rec.normal.y+1, rec.normal.z+1);
	}
	else
	{
		float3 n_dir = normalize(r.direction);
		float t = 0.5 * (n_dir.y + 1.0);
		return (1.0-t) * float3(1.0, 1.0, 1.0) + t * float3(0.5, 0.7, 1.0);
	}
}

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

	// Create ray
	float3 lower_left_corner = float3(-2.0, -1.0, -1.0);
	float3 horizontal = float3(4.0, 0.0, 0.0);
	float3 vertical = float3(0.0, 2.0, 0.0);

	Ray r;
	r.origin = float3(0.0, 0.0, 0.0);
	r.direction = lower_left_corner + uv.x * horizontal + uv.y * vertical;

	// Get final color
	final_color = float4(color(r), 1.0);

	// render final color
	return final_color;
}