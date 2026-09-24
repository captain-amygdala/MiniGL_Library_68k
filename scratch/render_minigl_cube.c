/*
 * Test & Verification: Rotating Textured 3D Cube with Multiple Dynamic Light Sources
 * Running on MiniGL API against VC4 Mock Hardware under vamos.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <proto/exec.h>

#include <mgl/gl.h>
#include "vc4_mock_hw.h"

#define WIDTH 256
#define HEIGHT 256
#define TEX_SIZE 64
#define NUM_FRAMES 24
#define SUBDIV 4   /* 4x4 quads per face for smooth Gouraud lighting & specular glints */

static uint32_t s_texture_pixels[TEX_SIZE * TEX_SIZE];

/* Generate an attractive test texture with colored quadrants and checkerboard */
static void generate_texture(void)
{
    for (int y = 0; y < TEX_SIZE; y++) {
        for (int x = 0; x < TEX_SIZE; x++) {
            /* 8x8 checkerboard pattern */
            int check = ((x / 8) ^ (y / 8)) & 1;
            /* Border frame (2 pixels) */
            int border = (x < 2 || x >= TEX_SIZE - 2 || y < 2 || y >= TEX_SIZE - 2);
            /* Center cross (2 pixels) */
            int cross = (abs(x - TEX_SIZE/2) < 2 || abs(y - TEX_SIZE/2) < 2);

            uint8_t r = 0, g = 0, b = 0, a = 255;

            if (border) {
                /* White border */
                r = 255; g = 255; b = 255;
            } else if (cross) {
                /* Yellow cross */
                r = 255; g = 230; b = 30;
            } else {
                /* Quadrant base colors with checkerboard brightness */
                int qx = (x >= TEX_SIZE / 2);
                int qy = (y >= TEX_SIZE / 2);
                int q = (qy << 1) | qx;

                switch (q) {
                case 0: /* Top-Left: Cyan / Teal */
                    r = check ? 40 : 10;
                    g = check ? 220 : 160;
                    b = check ? 255 : 200;
                    break;
                case 1: /* Top-Right: Magenta / Coral */
                    r = check ? 255 : 200;
                    g = check ? 40 : 20;
                    b = check ? 200 : 150;
                    break;
                case 2: /* Bottom-Left: Amber / Orange */
                    r = check ? 255 : 210;
                    g = check ? 160 : 110;
                    b = check ? 30 : 10;
                    break;
                case 3: /* Bottom-Right: Lime / Emerald */
                    r = check ? 80 : 30;
                    g = check ? 240 : 180;
                    b = check ? 60 : 20;
                    break;
                }
            }

            /* Store as RGBA8888 in Big Endian byte order (R, G, B, A) */
            uint8_t* p = (uint8_t*)&s_texture_pixels[y * TEX_SIZE + x];
            p[0] = r;
            p[1] = g;
            p[2] = b;
            p[3] = a;
        }
    }
}

/* 3D Vector & Matrix Math */
typedef struct { float x, y, z; } Vec3;

static inline Vec3 vec3(float x, float y, float z) {
    Vec3 v; v.x = x; v.y = y; v.z = z; return v;
}

static inline Vec3 vec3_add(Vec3 a, Vec3 b) {
    return vec3(a.x + b.x, a.y + b.y, a.z + b.z);
}

static inline Vec3 vec3_sub(Vec3 a, Vec3 b) {
    return vec3(a.x - b.x, a.y - b.y, a.z - b.z);
}

static inline float vec3_dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline Vec3 vec3_norm(Vec3 a) {
    float len = sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
    if (len > 1e-6f) {
        float inv = 1.0f / len;
        return vec3(a.x * inv, a.y * inv, a.z * inv);
    }
    return vec3(0.0f, 0.0f, 1.0f);
}

static inline Vec3 mat3_mul_vec(const float m[3][3], Vec3 v) {
    Vec3 r;
    r.x = m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z;
    r.y = m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z;
    r.z = m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z;
    return r;
}

/* Dynamic Light Sources */
typedef struct {
    Vec3 pos;       /* World space position */
    Vec3 color;     /* Diffuse light color */
    Vec3 spec;      /* Specular highlight color */
} Light;

#define NUM_LIGHTS 3

/* Compute vertex lighting (Ambient + Diffuse + Specular Blinn-Phong + Attenuation) */
static Vec3 compute_vertex_lighting(Vec3 v_local, Vec3 n_local, const float R[3][3],
                                   const Light lights[NUM_LIGHTS], Vec3 ambient)
{
    Vec3 v_world = mat3_mul_vec(R, v_local);
    Vec3 n_world = vec3_norm(mat3_mul_vec(R, n_local));
    Vec3 cam_pos = vec3(0.0f, 0.0f, 4.2f);
    Vec3 view_dir = vec3_norm(vec3_sub(cam_pos, v_world));

    Vec3 total = ambient;

    for (int i = 0; i < NUM_LIGHTS; i++) {
        Vec3 to_light = vec3_sub(lights[i].pos, v_world);
        float dist_sq = vec3_dot(to_light, to_light);
        float dist = sqrt(dist_sq);
        if (dist < 1e-4f) continue;

        Vec3 l_dir = vec3(to_light.x / dist, to_light.y / dist, to_light.z / dist);
        float att = 1.0f / (1.0f + 0.12f * dist + 0.04f * dist_sq);

        float n_dot_l = vec3_dot(n_world, l_dir);
        if (n_dot_l > 0.0f) {
            /* Diffuse Lambert term */
            float diff = att * n_dot_l;
            total.x += lights[i].color.x * diff;
            total.y += lights[i].color.y * diff;
            total.z += lights[i].color.z * diff;

            /* Specular Blinn-Phong term (h^12 via multiplications) */
            Vec3 half_vec = vec3_norm(vec3_add(l_dir, view_dir));
            float n_dot_h = vec3_dot(n_world, half_vec);
            if (n_dot_h > 0.0f) {
                float h2 = n_dot_h * n_dot_h;
                float h4 = h2 * h2;
                float h8 = h4 * h4;
                float spec_pow = h8 * h4;
                float spec = att * spec_pow * 1.8f;

                total.x += lights[i].spec.x * spec;
                total.y += lights[i].spec.y * spec;
                total.z += lights[i].spec.z * spec;
            }
        }
    }

    if (total.x > 1.0f) total.x = 1.0f;
    if (total.y > 1.0f) total.y = 1.0f;
    if (total.z > 1.0f) total.z = 1.0f;
    return total;
}

/* Draw a glowing 3D octahedron marker at light position */
static void draw_light_marker(float r)
{
    glBegin(GL_TRIANGLES);
    /* Top 4 facets */
    glVertex3f( 0.0f,  r,  0.0f); glVertex3f( r,  0.0f,  0.0f); glVertex3f( 0.0f,  0.0f,  r);
    glVertex3f( 0.0f,  r,  0.0f); glVertex3f( 0.0f,  0.0f,  r); glVertex3f(-r,  0.0f,  0.0f);
    glVertex3f( 0.0f,  r,  0.0f); glVertex3f(-r,  0.0f,  0.0f); glVertex3f( 0.0f,  0.0f, -r);
    glVertex3f( 0.0f,  r,  0.0f); glVertex3f( 0.0f,  0.0f, -r); glVertex3f( r,  0.0f,  0.0f);
    /* Bottom 4 facets */
    glVertex3f( 0.0f, -r,  0.0f); glVertex3f( 0.0f,  0.0f,  r); glVertex3f( r,  0.0f,  0.0f);
    glVertex3f( 0.0f, -r,  0.0f); glVertex3f(-r,  0.0f,  0.0f); glVertex3f( 0.0f,  0.0f,  r);
    glVertex3f( 0.0f, -r,  0.0f); glVertex3f( 0.0f,  0.0f, -r); glVertex3f(-r,  0.0f,  0.0f);
    glVertex3f( 0.0f, -r,  0.0f); glVertex3f( r,  0.0f,  0.0f); glVertex3f( 0.0f,  0.0f, -r);
    glEnd();
}

/* Draw one subdivided, lit face of the cube */
static void draw_subdivided_face(Vec3 p0, Vec3 p1, Vec3 p2, Vec3 p3, Vec3 normal,
                                const float R[3][3], const Light lights[NUM_LIGHTS], Vec3 ambient)
{
    for (int j = 0; j < SUBDIV; j++) {
        float v0 = (float)j / (float)SUBDIV;
        float v1 = (float)(j + 1) / (float)SUBDIV;

        for (int i = 0; i < SUBDIV; i++) {
            float u0 = (float)i / (float)SUBDIV;
            float u1 = (float)(i + 1) / (float)SUBDIV;

            /* Bilinear interpolation of corner points */
            Vec3 c00 = vec3_add(vec3_add(vec3((1-u0)*(1-v0)*p0.x, (1-u0)*(1-v0)*p0.y, (1-u0)*(1-v0)*p0.z),
                                         vec3(u0*(1-v0)*p1.x,     u0*(1-v0)*p1.y,     u0*(1-v0)*p1.z)),
                                vec3_add(vec3(u0*v0*p2.x,         u0*v0*p2.y,         u0*v0*p2.z),
                                         vec3((1-u0)*v0*p3.x,     (1-u0)*v0*p3.y,     (1-u0)*v0*p3.z)));

            Vec3 c10 = vec3_add(vec3_add(vec3((1-u1)*(1-v0)*p0.x, (1-u1)*(1-v0)*p0.y, (1-u1)*(1-v0)*p0.z),
                                         vec3(u1*(1-v0)*p1.x,     u1*(1-v0)*p1.y,     u1*(1-v0)*p1.z)),
                                vec3_add(vec3(u1*v0*p2.x,         u1*v0*p2.y,         u1*v0*p2.z),
                                         vec3((1-u1)*v0*p3.x,     (1-u1)*v0*p3.y,     (1-u1)*v0*p3.z)));

            Vec3 c11 = vec3_add(vec3_add(vec3((1-u1)*(1-v1)*p0.x, (1-u1)*(1-v1)*p0.y, (1-u1)*(1-v1)*p0.z),
                                         vec3(u1*(1-v1)*p1.x,     u1*(1-v1)*p1.y,     u1*(1-v1)*p1.z)),
                                vec3_add(vec3(u1*v1*p2.x,         u1*v1*p2.y,         u1*v1*p2.z),
                                         vec3((1-u1)*v1*p3.x,     (1-u1)*v1*p3.y,     (1-u1)*v1*p3.z)));

            Vec3 c01 = vec3_add(vec3_add(vec3((1-u0)*(1-v1)*p0.x, (1-u0)*(1-v1)*p0.y, (1-u0)*(1-v1)*p0.z),
                                         vec3(u0*(1-v1)*p1.x,     u0*(1-v1)*p1.y,     u0*(1-v1)*p1.z)),
                                vec3_add(vec3(u0*v1*p2.x,         u0*v1*p2.y,         u0*v1*p2.z),
                                         vec3((1-u0)*v1*p3.x,     (1-u0)*v1*p3.y,     (1-u0)*v1*p3.z)));

            Vec3 col00 = compute_vertex_lighting(c00, normal, R, lights, ambient);
            Vec3 col10 = compute_vertex_lighting(c10, normal, R, lights, ambient);
            Vec3 col11 = compute_vertex_lighting(c11, normal, R, lights, ambient);
            Vec3 col01 = compute_vertex_lighting(c01, normal, R, lights, ambient);

            glBegin(GL_QUADS);
            glColor3f(col00.x, col00.y, col00.z); glTexCoord2f(u0, v0); glVertex3f(c00.x, c00.y, c00.z);
            glColor3f(col10.x, col10.y, col10.z); glTexCoord2f(u1, v0); glVertex3f(c10.x, c10.y, c10.z);
            glColor3f(col11.x, col11.y, col11.z); glTexCoord2f(u1, v1); glVertex3f(c11.x, c11.y, c11.z);
            glColor3f(col01.x, col01.y, col01.z); glTexCoord2f(u0, v1); glVertex3f(c01.x, c01.y, c01.z);
            glEnd();
        }
    }
}

/* Draw full subdivided cube with dynamic lighting */
static void draw_lit_cube(const float R[3][3], const Light lights[NUM_LIGHTS], Vec3 ambient)
{
    /* 1. Front (+Z) */
    draw_subdivided_face(vec3(-1.0f, -1.0f,  1.0f), vec3( 1.0f, -1.0f,  1.0f),
                         vec3( 1.0f,  1.0f,  1.0f), vec3(-1.0f,  1.0f,  1.0f),
                         vec3(0.0f, 0.0f, 1.0f), R, lights, ambient);

    /* 2. Back (-Z) */
    draw_subdivided_face(vec3( 1.0f, -1.0f, -1.0f), vec3(-1.0f, -1.0f, -1.0f),
                         vec3(-1.0f,  1.0f, -1.0f), vec3( 1.0f,  1.0f, -1.0f),
                         vec3(0.0f, 0.0f, -1.0f), R, lights, ambient);

    /* 3. Top (+Y) */
    draw_subdivided_face(vec3(-1.0f,  1.0f,  1.0f), vec3( 1.0f,  1.0f,  1.0f),
                         vec3( 1.0f,  1.0f, -1.0f), vec3(-1.0f,  1.0f, -1.0f),
                         vec3(0.0f, 1.0f, 0.0f), R, lights, ambient);

    /* 4. Bottom (-Y) */
    draw_subdivided_face(vec3(-1.0f, -1.0f, -1.0f), vec3( 1.0f, -1.0f, -1.0f),
                         vec3( 1.0f, -1.0f,  1.0f), vec3(-1.0f, -1.0f,  1.0f),
                         vec3(0.0f, -1.0f, 0.0f), R, lights, ambient);

    /* 5. Right (+X) */
    draw_subdivided_face(vec3( 1.0f, -1.0f,  1.0f), vec3( 1.0f, -1.0f, -1.0f),
                         vec3( 1.0f,  1.0f, -1.0f), vec3( 1.0f,  1.0f,  1.0f),
                         vec3(1.0f, 0.0f, 0.0f), R, lights, ambient);

    /* 6. Left (-X) */
    draw_subdivided_face(vec3(-1.0f, -1.0f, -1.0f), vec3(-1.0f, -1.0f,  1.0f),
                         vec3(-1.0f,  1.0f,  1.0f), vec3(-1.0f,  1.0f, -1.0f),
                         vec3(-1.0f, 0.0f, 0.0f), R, lights, ambient);
}

int main(int argc, char** argv)
{
    printf("=====================================================\n");
    printf(" MiniGL 3D Textured Cube with 3 Dynamic Light Sources \n");
    printf("=====================================================\n");

    /* 1. Create MiniGL context */
    GLcontext context = mglCreateContext(0, 0, WIDTH, HEIGHT);
    if (!context) {
        printf("FAILED: mglCreateContext\n");
        return 1;
    }
    printf("[1/5] MiniGL context created (%dx%d, mock active: %d)\n",
           WIDTH, HEIGHT, v3d_mock_is_active());

    /* 2. Create and upload texture */
    generate_texture();
    GLuint tex_id = 0;
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEX_SIZE, TEX_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, s_texture_pixels);

    /* Dump texture as PPM for verification */
    v3d_mock_dump_ppm("scratch/cube_texture.ppm", s_texture_pixels, TEX_SIZE, TEX_SIZE);
    printf("[2/5] %dx%d RGBA8 Texture generated & uploaded (ID: %u)\n", TEX_SIZE, TEX_SIZE, (unsigned)tex_id);

    /* 3. Configure OpenGL State */
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glViewport(0, 0, WIDTH, HEIGHT);
    printf("[3/5] OpenGL state: Depth test, Texturing (GL_MODULATE), Viewport 0..%d\n", WIDTH);

    /* 4. Render rotating frames with dynamic lights */
    printf("[4/5] Rendering %d frames with 3 orbiting light sources...\n", NUM_FRAMES);

    for (int frame = 0; frame < NUM_FRAMES; frame++) {
        float angle_y = (float)frame * (360.0f / (float)NUM_FRAMES);
        float angle_x = 25.0f;
        float angle_z = (float)frame * (180.0f / (float)NUM_FRAMES);
        float t = (float)frame * (2.0f * 3.14159265f / (float)NUM_FRAMES);

        /* 3 Dynamic orbiting light sources: Key, Fill, and Rim */
        Light lights[NUM_LIGHTS] = {
            /* 1. Golden Key Light: Horizontal orbit */
            {
                .pos   = vec3(2.6f * cos(t), 1.2f + 0.4f * sin(t), 2.6f * sin(t)),
                .color = vec3(1.0f, 0.85f, 0.45f),
                .spec  = vec3(1.0f, 0.95f, 0.70f)
            },
            /* 2. Cold Cyan Fill Light: Front-left diagonal orbit */
            {
                .pos   = vec3(-2.2f * cos(t * 0.5f), -0.5f + 0.6f * sin(t * 0.5f), 1.8f + 0.8f * cos(t * 0.5f)),
                .color = vec3(0.35f, 0.75f, 1.0f),
                .spec  = vec3(0.60f, 0.85f, 1.0f)
            },
            /* 3. Vivid Magenta Rim Light: High orbit creating edge glints */
            {
                .pos   = vec3(1.8f * sin(t), 2.5f * cos(t * 0.6f), -1.8f * cos(t)),
                .color = vec3(1.0f, 0.25f, 0.75f),
                .spec  = vec3(1.0f, 0.60f, 0.90f)
            }
        };
        Vec3 ambient = vec3(0.28f, 0.28f, 0.32f);

        /* Compute 3x3 rotation matrix R = Rx * Ry * Rz */
        float ax = angle_x * (3.14159265f / 180.0f);
        float ay = angle_y * (3.14159265f / 180.0f);
        float az = (angle_z * 0.3f) * (3.14159265f / 180.0f);

        float cx = cos(ax), sx = sin(ax);
        float cy = cos(ay), sy = sin(ay);
        float cz = cos(az), sz = sin(az);

        float B[3][3];
        B[0][0] = cy * cz;   B[0][1] = -cy * sz;  B[0][2] = sy;
        B[1][0] = sz;        B[1][1] = cz;        B[1][2] = 0.0f;
        B[2][0] = -sy * cz;  B[2][1] = sy * sz;   B[2][2] = cy;

        float R[3][3];
        R[0][0] = B[0][0];
        R[0][1] = B[0][1];
        R[0][2] = B[0][2];

        R[1][0] = cx * B[1][0] - sx * B[2][0];
        R[1][1] = cx * B[1][1] - sx * B[2][1];
        R[1][2] = cx * B[1][2] - sx * B[2][2];

        R[2][0] = sx * B[1][0] + cx * B[2][0];
        R[2][1] = sx * B[1][1] + cx * B[2][1];
        R[2][2] = sx * B[1][2] + cx * B[2][2];

        /* Clear color: Dark slate void */
        glClearColor(0.08f, 0.09f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        /* Projection: 45 degree perspective */
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(45.0f, (GLfloat)WIDTH / (GLfloat)HEIGHT, 1.0f, 50.0f);

        /* ModelView: camera back by 4.2 units */
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(0.0f, 0.0f, -4.2f);

        /* 1. Draw glowing light markers in world space (untextured) */
        glDisable(GL_TEXTURE_2D);
        for (int i = 0; i < NUM_LIGHTS; i++) {
            glPushMatrix();
            glTranslatef(lights[i].pos.x, lights[i].pos.y, lights[i].pos.z);
            glColor3f(lights[i].color.x, lights[i].color.y, lights[i].color.z);
            draw_light_marker(0.18f);
            glPopMatrix();
        }
        glEnable(GL_TEXTURE_2D);

        /* 2. Rotate and draw the dynamically illuminated textured cube */
        glPushMatrix();
        glRotatef(angle_x, 1.0f, 0.0f, 0.0f);
        glRotatef(angle_y, 0.0f, 1.0f, 0.0f);
        glRotatef(angle_z * 0.3f, 0.0f, 0.0f, 1.0f);

        draw_lit_cube(R, lights, ambient);

        glPopMatrix();

        /* Present */
        mglSwitchDisplay();

        /* Dump frame */
        char filename[64];
        snprintf(filename, sizeof(filename), "scratch/cube_frame_%02d.ppm", frame);
        int fb_w = 0, fb_h = 0;
        void* fb = v3d_mock_get_framebuffer(&fb_w, &fb_h);
        if (fb && fb_w > 0 && fb_h > 0) {
            v3d_mock_dump_ppm(filename, fb, fb_w, fb_h);
        }

        if (frame % 6 == 0 || frame == NUM_FRAMES - 1) {
            printf("      Frame %02d/%d rendered (angle Y=%d deg)\n", frame, NUM_FRAMES, (int)angle_y);
        }
    }

    /* 5. Print statistics and clean up */
    V3DMockStats stats;
    v3d_mock_get_stats(&stats);
    printf("[5/5] Rendering Complete!\n");
    printf("   - Total Binning Jobs:  %lu\n", (ULONG)stats.binning_jobs);
    printf("   - Total Render Jobs:   %lu\n", (ULONG)stats.render_jobs);
    printf("   - Total Packets:       %lu\n", (ULONG)stats.packets_total);
    printf("   - Total Primitives:    %lu\n", (ULONG)stats.primitives_total);
    printf("   - Pixels Rasterized:   %lu\n", (ULONG)stats.pixels_rasterized);
    printf("   - Errors Detected:     %lu\n", (ULONG)stats.errors_detected);

    mglDeleteContext();
    printf("Context deleted cleanly. Done!\n");
    return 0;
}
