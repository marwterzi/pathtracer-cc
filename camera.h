#ifndef CAMERA_H
#define CAMERA_H

#include "hittable.h"
#include "material.h"

class camera {
  public:
    /* Public Camera Parameters Here */
    double aspect_ratio = 1.0;  // Ratio of image width over height
    int    image_width  = 100;  // Rendered image width in pixel count
    int    samples_per_pixel = 10;   // Count of random samples for each pixel
    int    max_depth         = 10;   // Maximum number of ray bounces into scene

    double vfov = 90;  // Vertical view angle (field of view)

    point3 lookfrom = point3(0,0,0);   // Point camera is looking from
    point3 lookat   = point3(0,0,-1);  // Point camera is looking at
    vec3   vup      = vec3(0,1,0);     // Camera-relative "up" direction

    double defocus_angle = 0;  // Variation angle of rays through each pixel
    double focus_dist = 10;    // Distance from camera lookfrom point to plane of perfect focus

    void render(const hittable& world) {
        initialize();
        
        // Render
        std::cout << "P3\n" << image_width << ' ' << image_height << "\n255\n";

        for (int j = 0; j < image_height; j++) {
            // add progress indicator for long rendering
            std::clog << "\rScanlines remaining: " << (image_height - j) << ' ' << std::flush;
            for (int i = 0; i < image_width; i++) {
                color pixel_color(0,0,0);
                for (int sample = 0; sample < samples_per_pixel; sample++) {
                    ray r = get_ray(i, j);
                    pixel_color += ray_color(r, max_depth, world);
                }
                write_color(std::cout, pixel_samples_scale * pixel_color);
            }
        }
        std::clog << "\rDone.                 \n";
    }

  private:
    /* Private Camera Variables Here */
    int    image_height;   // Rendered image height
    double pixel_samples_scale;  // Color scale factor for a sum of pixel samples
    point3 center;         // Camera center
    point3 pixel00_loc;    // Location of pixel 0, 0
    vec3   pixel_delta_u;  // Offset to pixel to the right
    vec3   pixel_delta_v;  // Offset to pixel below
    vec3   u, v, w;              // Camera frame basis vectors

    vec3   defocus_disk_u;       // Defocus disk horizontal radius
    vec3   defocus_disk_v;       // Defocus disk vertical radius
    
    void initialize() {
        // Calculate the image height, and ensure that it's at least 1.
        image_height = int(image_width / aspect_ratio);
        image_height = (image_height < 1) ? 1 : image_height;
        
        pixel_samples_scale = 1.0 / samples_per_pixel;

        center = lookfrom;

        //auto focal_length = (lookfrom - lookat).length(); --> before integrating defocus blur
        /* Adjustable Camera = Adjustable Field of View */
        auto theta = degrees_to_radians(vfov);
        auto h = std::tan(theta/2);
        auto viewport_height = 2 * h * focus_dist;
        auto viewport_width = viewport_height * (double(image_width)/image_height);
        
        // Calculate the u,v,w unit basis vectors for the camera coordinate frame.
        w = unit_vector(lookfrom - lookat);
        u = unit_vector(cross(vup, w));
        v = cross(w, u);

        // Calculate the vectors across the horizontal and down the vertical viewport edges.
        vec3 viewport_u = viewport_width * u;    // Vector across viewport horizontal edge
        vec3 viewport_v = viewport_height * -v;  // Vector down viewport vertical edge

        // Calculate the horizontal and vertical delta vectors from pixel to pixel.
        pixel_delta_u = viewport_u / image_width;
        pixel_delta_v = viewport_v / image_height;

        // Calculate the location of the upper left pixel.
        auto viewport_upper_left = center - (focus_dist * w) - viewport_u/2 - viewport_v/2;
        pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

         // Calculate the camera defocus disk basis vectors.
        auto defocus_radius = focus_dist * std::tan(degrees_to_radians(defocus_angle / 2));
        defocus_disk_u = u * defocus_radius;
        defocus_disk_v = v * defocus_radius;
    }

    ray get_ray(int i, int j) const {
        // Construct a camera ray originating from the defocus disk and directed at a randomly
        // sampled point around the pixel location i, j.
        auto offset = sample_square();
        auto pixel_sample = pixel00_loc
                          + ((i + offset.x()) * pixel_delta_u)
                          + ((j + offset.y()) * pixel_delta_v);

        // defocus_angle <= 0 for excluding bluriness
        auto ray_origin = (defocus_angle <= 0) ? center : defocus_disk_sample();
        auto ray_direction = pixel_sample - ray_origin;

        return ray(ray_origin, ray_direction);
    }

    vec3 sample_square() const {
        // Returns the vector to a random point in the [-.5,-.5]-[+.5,+.5] unit square.
        // keep in mind random_double() returns a value in [0, 1].
        return vec3(random_double() - 0.5, random_double() - 0.5, 0);
    }
    
    point3 defocus_disk_sample() const {
        // Returns a random point in the camera defocus disk.
        auto p = random_in_unit_disk();
        return center + (p[0] * defocus_disk_u) + (p[1] * defocus_disk_v);
    }

    color ray_color(const ray& r, int depth, const hittable& world) const {
        // If we've exceeded the ray bounce limit, no more light is gathered.
        if (depth <= 0) // to avoid the recursion from blowing up the stack
            return color(0,0,0);

        hit_record rec;
        /*
            When a ray hits a surface, the reflected ray has its origin at the surface (the point of hit) 
            and if we then solve the intersection equation for this new ray, we will get as one of the solutions t=0 (ideally) 
            since the reflected ray initially "sits" on the surface off of which it is reflected 
            and therefore we have intersection there. To discard this solution we consider a hit only if t > t_min for t_min=0. 
            However, due to floating point precision limitations, we never actually get t=0 as solution, but something like t=0.00000001, 
            which would pass the test t > t_min and we would have a false second hit causing color attenuation ("shadow"). 
            So by changing t_min from 0 to e.g. 0.001 we can solve this problem.
        */
        if (world.hit(r, interval(0.001, infinity), rec)) {
            // normal's components belong to [-1,1], here it maps them to [0,1] to use as colors
            //return 0.5 * (rec.normal + color(1,1,1));

            // on Diffuse Material, the color thats getting returned 
            // is the 50% of the color from a bounce (the other 50% is getting absorbed)
            // the more bounces, the darker it becomes
            
            /* Uniform Scattering */
            // vec3 direction = random_on_hemisphere(rec.normal);
            /* Lambertian Scattering 
                vec3 direction = rec.normal + random_unit_vector();
                return 0.5 * ray_color(ray(rec.p, direction), depth-1, world); // 0.5 * ... * sky color
            */
            ray scattered;
            color attenuation;
            // scatter function is (as i see it) a bool one in order to have the capability of
            // adding a possibility of total or mere absorption of the scattered ray
            if (rec.mat->scatter(r, rec, attenuation, scattered))
                /* attenuation --> from what color channels the material absorbs */
                return attenuation * ray_color(scattered, depth-1, world);
            return color(0,0,0);
        }
        
        // normanlize so unit_direction.y() goes from -1.0 to 1.0
        vec3 unit_direction = unit_vector(r.direction());
        // The range -1.0 to 1.0 is awkward for blending. Remap it to 0.0 to 1.0.
        auto a = 0.5*(unit_direction.y() + 1.0);
        // color(1.0, 1.0, 1.0) is white and color(0.5, 0.7, 1.0) is blue
        return (1.0-a)*color(1.0, 1.0, 1.0) + a*color(0.5, 0.7, 1.0);
        /*
            so a is the weight along the white-blue lerp 
            a=0.0 == white, a=0.5 == 50/50 blend, a=1.0 == blue

            and a is dependent on the y coordinate of the ray 
            because sky in this model changes color from top to bottom
        */
    }
};

#endif
