#ifndef MATERIAL_H
#define MATERIAL_H

#include "hittable.h"

// abstract
class material {
  public:
    virtual ~material() = default;

    virtual bool scatter(
        const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered
    ) const {
        return false;
    }
};

/*
    Lambertian (diffuse) reflectance can either always scatter 
    and attenuate light according to its reflectance R
    , or it can sometimes scatter (with probability 1−R) 
    with no attenuation (where a ray that isn't scattered is just absorbed into the material). 
    It could also be a mixture of both those strategies. We will choose to always scatter.
*/

class lambertian : public material { // Diffuse Material
  public:
    lambertian(const color& albedo) : albedo(albedo) {}

    bool scatter(const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered)
    const override {
        auto scatter_direction = rec.normal + random_unit_vector();
        
        // Catch degenerate scatter direction
        if (scatter_direction.near_zero())
            scatter_direction = rec.normal;

        scattered = ray(rec.p, scatter_direction);
        attenuation = albedo;
        return true;
    }

  private:
    color albedo;
};

class metal : public material {
  public:
    metal(const color& albedo, double fuzz) : albedo(albedo), fuzz(fuzz < 1 ? fuzz : 1) {}

    bool scatter(const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered)
    const override {
        vec3 reflected = reflect(r_in.direction(), rec.normal); // perfect mirroring, gives the same angle as the og ray
        reflected = unit_vector(reflected) + (fuzz * random_unit_vector()); // changes the endpoint of the perfect mirrored ray, so it creates blurriness
        scattered = ray(rec.p, reflected);
        attenuation = albedo;
        /*
          If dot(scattered, normal) < 0 the scattered ray goes below the surface, 
          so return false which tells the material to absorb it (return black).
          This only happens with high fuzz values or grazing rays — rays that hit the surface at a very shallow angle. 
        */
        return (dot(scattered.direction(), rec.normal) > 0); 
    }

  private:
    color albedo;
    double fuzz;
};

class dielectric : public material {
  public:
    dielectric(double refraction_index) : refraction_index(refraction_index) {}

    bool scatter(const ray& r_in, const hit_record& rec, color& attenuation, ray& scattered)
    const override {
        // Attenuation is always 1 — the glass surface absorbs nothing.
        attenuation = color(1.0, 1.0, 1.0);

        // air's refraction_index = 1.0
        /* When entering glass (ray is outside, hitting front face):
          coming from: air   (n = 1.0)
          going into:  glass (n = 1.5) -> ratio = glass/air = 1.5/1.0 
          When exiting glass (ray is inside, hitting back face): ratio flips. */
        double ri = rec.front_face ? (1.0/refraction_index) : refraction_index;

        vec3 unit_direction = unit_vector(r_in.direction());
        double cos_theta = std::fmin(dot(-unit_direction, rec.normal), 1.0);
        double sin_theta = std::sqrt(1.0 - cos_theta*cos_theta);

        bool cannot_refract = ri * sin_theta > 1.0; // this equals sin_theta_refracted in snell's law
        vec3 direction;

        /* this applies to angles from 60 degrees and above 
           usually happens inside the glass object */
        if (cannot_refract || reflectance(cos_theta, ri) > random_double())
            direction = reflect(unit_direction, rec.normal); // total internal reflection
        else
        /*  this applies to smaller angles, meaning the ray is close to normal
            as it happens with rays exiting one sphere and hitting the closest glass object 
            , in this case a ray is created inside the glass object (refracted), which 
            then will hit the back face of the object as it travels to the other side of the object */
            direction = refract(unit_direction, rec.normal, ri);

        scattered = ray(rec.p, direction);
        return true;
    }

  private:
    // Refractive index in vacuum or air, or the ratio of the material's refractive index over
    // the refractive index of the enclosing media
    double refraction_index;
    static double reflectance(double cosine, double refraction_index) {
      /*
        Now real glass has reflectivity that varies with angle 
        — look at a window at a steep angle and it becomes a mirror.
      */
        // Use Schlick's approximation for reflectance.
        auto r0 = (1 - refraction_index) / (1 + refraction_index);
        r0 = r0*r0;
        return r0 + (1-r0)*std::pow((1 - cosine),5);
    }
};

#endif