#ifndef HITTABLE_H
#define HITTABLE_H

class material;

class hit_record { // just to store some info instead of those used as arguments 
  public:
    point3 p;
    vec3 normal;
    /*
      hit_record is just a way to stuff a bunch of arguments 
      into a class so we can send them as a group. 
      When a ray hits a surface (a particular sphere for example), 
      the material pointer in the hit_record 
      will be set to point at the material pointer 
      the sphere was given when it was set up in main() when we start.
    */
    shared_ptr<material> mat;
    double t;
    bool front_face;

    void set_face_normal(const ray& r, const vec3& outward_normal) {
        // Sets the hit record normal vector.
        // NOTE: the parameter `outward_normal` is assumed to have unit length.

        // we want normal to point to ray's origin 
        front_face = dot(r.direction(), outward_normal) < 0;  // ray is outside, normal points correctly here
        normal = front_face ? outward_normal : -outward_normal;
    }
};

class hittable {
  public:
    virtual ~hittable() = default;

    virtual bool hit(const ray& r, interval ray_t, hit_record& rec) const = 0;
};

#endif