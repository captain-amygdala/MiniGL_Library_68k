/*
 * MiniGL Window Demo: Rotating 3D Textured Cube with Dynamic 3-Point Lighting
 * Target: AmigaOS 3.x / Emu68 on PiStorm (VideoCore IV / VC4 or V3D)
 *
 * Runs in an Intuition window on the Workbench / RTG desktop.
 * Press ESC or click the window Close gadget to exit.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <intuition/intuition.h>

#include <mgl/gl.h>

#define WIN_WIDTH   320
#define WIN_HEIGHT  240
#define TEX_SIZE    64
#define SUBDIV      4    /* 4x4 quads per face */

/* ESC key scan code on Amiga keyboard */
#define RAWKEY_ESC  0x45

extern struct IntuitionBase *IntuitionBase;
extern struct GfxBase *GfxBase;

/* 3D Vector Math */
typedef struct {
    float x, y, z;
} Vec3;

static inline Vec3 vec3(float x, float y, float z) {
    Vec3 v = {x, y, z};
    return v;
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
    glBegin(GL_QUADS);
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

            glColor3f(col00.x, col00.y, col00.z); glTexCoord2f(u0, v0); glVertex3f(c00.x, c00.y, c00.z);
            glColor3f(col10.x, col10.y, col10.z); glTexCoord2f(u1, v0); glVertex3f(c10.x, c10.y, c10.z);
            glColor3f(col11.x, col11.y, col11.z); glTexCoord2f(u1, v1); glVertex3f(c11.x, c11.y, c11.z);
            glColor3f(col01.x, col01.y, col01.z); glTexCoord2f(u0, v1); glVertex3f(c01.x, c01.y, c01.z);
        }
    }
    glEnd();
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

/* Procedural texture generation */
static uint32_t s_texture_data[TEX_SIZE * TEX_SIZE];

static void init_cube_texture(void)
{
    for (int y = 0; y < TEX_SIZE; y++) {
        for (int x = 0; x < TEX_SIZE; x++) {
            int check = ((x / 8) ^ (y / 8)) & 1;
            int border = (x < 2 || x >= TEX_SIZE - 2 || y < 2 || y >= TEX_SIZE - 2);
            int cross = (abs(x - TEX_SIZE / 2) < 2 || abs(y - TEX_SIZE / 2) < 2);

            uint8_t r = 0, g = 0, b = 0, a = 255;

            if (border) {
                r = 255; g = 255; b = 255;
            } else if (cross) {
                r = 255; g = 230; b = 40;
            } else {
                int qx = (x >= TEX_SIZE / 2);
                int qy = (y >= TEX_SIZE / 2);
                int q = (qy << 1) | qx;

                switch (q) {
                case 0: /* Top-Left: Cyan */
                    r = check ? 40 : 15;
                    g = check ? 210 : 160;
                    b = check ? 255 : 210;
                    break;
                case 1: /* Top-Right: Magenta */
                    r = check ? 255 : 210;
                    g = check ? 40 : 20;
                    b = check ? 200 : 150;
                    break;
                case 2: /* Bottom-Left: Amber */
                    r = check ? 255 : 210;
                    g = check ? 170 : 120;
                    b = check ? 30 : 15;
                    break;
                case 3: /* Bottom-Right: Emerald */
                    r = check ? 30 : 15;
                    g = check ? 230 : 170;
                    b = check ? 100 : 50;
                    break;
                }
            }

            s_texture_data[y * TEX_SIZE + x] =
                ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | (uint32_t)a;
        }
    }
}

/* Set minimum stack for AmigaOS C runtime (clib2 / libnix) */
unsigned long __stack = 262144;
unsigned long __stack_size = 262144;

extern void kprintf(const char *format, ...);

int main(int argc, char **argv)
{
    unsigned long maxFrames = 0;
    if (argc > 1) {
        maxFrames = strtoul(argv[1], NULL, 0);
    }

    printf("=========================================\n");
    printf("MiniGL VC4 Windowed Hardware Test\n");
    printf("Target: VideoCore IV (VC4) / Emu68 PiStorm\n");
    printf("=========================================\n");
    kprintf("DEMO: Starting cube_window_demo (maxFrames=%lu)\n", maxFrames);

    /* 1. Initialize MiniGL subsystem */
    if (!MGLInit()) {
        fprintf(stderr, "Error: MGLInit failed!\n");
        kprintf("DEMO: Error: MGLInit failed!\n");
        return 20;
    }
    kprintf("DEMO: MGLInit succeeded\n");

    /* 2. Open an Intuition window on the default / Workbench screen */
    struct TagItem winTags[] = {
        {WA_InnerWidth,   WIN_WIDTH},
        {WA_InnerHeight,  WIN_HEIGHT},
        {WA_Left,         60},
        {WA_Top,          40},
        {WA_Title,        (ULONG)"MiniGL VC4 - 3D Lit Cube Demo"},
        {WA_DragBar,      TRUE},
        {WA_CloseGadget,  TRUE},
        {WA_DepthGadget,  TRUE},
        {WA_Activate,     TRUE},
        {WA_IDCMP,        IDCMP_CLOSEWINDOW | IDCMP_RAWKEY},
        {TAG_DONE,        0}
    };

    struct Window *win = OpenWindowTagList(NULL, winTags);
    if (!win) {
        fprintf(stderr, "Error: Could not open Intuition window!\n");
        MGLTerm();
        return 20;
    }

    printf("Intuition window opened (%ldx%ld inner area)\n",
           (long)WIN_WIDTH, (long)WIN_HEIGHT);

    /* 3. Create MiniGL context bound to this window */
    GLcontext ctx = mglCreateContextFromWindow(win);
    if (!ctx) {
        fprintf(stderr, "Error: mglCreateContextFromWindow failed!\n");
        kprintf("DEMO: Error: mglCreateContextFromWindow failed!\n");
        CloseWindow(win);
        MGLTerm();
        return 20;
    }

    printf("MiniGL context created successfully!\n");
    kprintf("DEMO: MiniGL context created successfully (ctx=%08lx)!\n", (ULONG)ctx);

    /* 4. Setup OpenGL / MiniGL viewport & state */
    glViewport(0, 0, WIN_WIDTH, WIN_HEIGHT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0f, (GLfloat)WIN_WIDTH / (GLfloat)WIN_HEIGHT, 1.0f, 50.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    /* Render states */
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glShadeModel(GL_SMOOTH);
    glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
    glClearDepth(1.0);

    /* 5. Upload procedural texture */
    init_cube_texture();
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEX_SIZE, TEX_SIZE, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, s_texture_data);
    glEnable(GL_TEXTURE_2D);

    printf("Starting render loop. Close window or press ESC to exit...\n");

    /* 6. Dynamic lighting setup */
    Vec3 ambient = vec3(0.12f, 0.12f, 0.16f);

    int running = 1;
    float rotX = 18.0f, rotY = 25.0f, rotZ = 0.0f;
    float animTime = 0.0f;
    unsigned long frameCount = 0;

    while (running) {
        /* Process Intuition IDCMP events */
        struct IntuiMessage *msg;
        while ((msg = (struct IntuiMessage *)GetMsg(win->UserPort))) {
            ULONG msgClass = msg->Class;
            UWORD msgCode  = msg->Code;
            ReplyMsg((struct Message *)msg);

            if (msgClass == IDCMP_CLOSEWINDOW) {
                running = 0;
            } else if (msgClass == IDCMP_RAWKEY) {
                if (msgCode == RAWKEY_ESC) {
                    running = 0;
                }
            }
        }

        if (!running)
            break;

        /* Clear frame */
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        /* 3 Dynamic light sources */
        Light lights[NUM_LIGHTS];

        /* Light 0: Golden Key Light orbiting in X-Z plane */
        lights[0].pos = vec3(2.4f * cos(animTime * 0.045f),
                             1.2f + 0.6f * sin(animTime * 0.03f),
                             2.4f * sin(animTime * 0.045f));
        lights[0].color = vec3(1.0f, 0.85f, 0.50f);
        lights[0].spec  = vec3(1.0f, 0.95f, 0.70f);

        /* Light 1: Cool Cyan Fill Light orbiting counter-clockwise */
        lights[1].pos = vec3(2.2f * cos(-animTime * 0.035f + 2.2f),
                             -1.0f + 0.5f * cos(animTime * 0.04f),
                             2.2f * sin(-animTime * 0.035f + 2.2f));
        lights[1].color = vec3(0.20f, 0.65f, 1.0f);
        lights[1].spec  = vec3(0.40f, 0.80f, 1.0f);

        /* Light 2: Vivid Magenta Rim Light for edge highlights */
        lights[2].pos = vec3(2.0f * cos(animTime * 0.025f + 4.0f),
                             0.8f * sin(animTime * 0.05f),
                             -2.2f + 0.6f * cos(animTime * 0.03f));
        lights[2].color = vec3(0.95f, 0.20f, 0.85f);
        lights[2].spec  = vec3(1.0f, 0.50f, 0.95f);

        /* Compute Rotation Matrix R = Rz * Ry * Rx */
        float radX = rotX * (3.14159265f / 180.0f);
        float radY = rotY * (3.14159265f / 180.0f);
        float radZ = rotZ * (3.14159265f / 180.0f);

        float cx = cos(radX), sx = sin(radX);
        float cy = cos(radY), sy = sin(radY);
        float cz = cos(radZ), sz = sin(radZ);

        float R[3][3];
        R[0][0] = cy * cz;
        R[0][1] = -cy * sz;
        R[0][2] = sy;

        R[1][0] = sx * sy * cz + cx * sz;
        R[1][1] = -sx * sy * sz + cx * cz;
        R[1][2] = -sx * cy;

        R[2][0] = -cx * sy * cz + sx * sz;
        R[2][1] = cx * sy * sz + sx * cz;
        R[2][2] = cx * cy;

        /* Position camera and draw rotating cube */
        glLoadIdentity();
        glTranslatef(0.0f, 0.0f, -4.2f);

        /* Draw orbiting 3D light markers (untextured) */
        glDisable(GL_TEXTURE_2D);
        for (int i = 0; i < NUM_LIGHTS; i++) {
            glPushMatrix();
            glTranslatef(lights[i].pos.x, lights[i].pos.y, lights[i].pos.z);
            glColor3f(lights[i].color.x, lights[i].color.y, lights[i].color.z);
            draw_light_marker(0.10f);
            glPopMatrix();
        }
        glEnable(GL_TEXTURE_2D);

        /* Draw lit textured cube */
        glPushMatrix();
        glRotatef(rotX, 1.0f, 0.0f, 0.0f);
        glRotatef(rotY, 0.0f, 1.0f, 0.0f);
        glRotatef(rotZ, 0.0f, 0.0f, 1.0f);

        draw_lit_cube(R, lights, ambient);

        glPopMatrix();

        /* Present rendered frame into window */
        mglSwitchDisplay();

        /* Update animations */
        rotX += 1.2f;
        rotY += 1.8f;
        rotZ += 0.7f;
        animTime += 1.0f;
        frameCount++;

        if (frameCount == 1 || (frameCount % 10) == 0) {
            kprintf("DEMO: frame %lu rendered\n", frameCount);
        }

        if ((frameCount % 60) == 0) {
            printf("Rendered %lu frames...\n", frameCount);
        }

        if (maxFrames > 0 && frameCount >= maxFrames) {
            kprintf("DEMO: reached maxFrames (%lu), exiting loop\n", maxFrames);
            running = 0;
            break;
        }
    }

    printf("Exiting... Total frames rendered: %lu\n", frameCount);
    kprintf("DEMO: Exiting... Total frames rendered: %lu\n", frameCount);

    /* 7. Cleanup and shutdown */
    glDeleteTextures(1, &texID);
    mglDeleteContext();
    if (win) {
        if (win->UserPort) {
            struct Message *msg;
            Forbid();
            while ((msg = GetMsg(win->UserPort))) {
                ReplyMsg(msg);
            }
            win->UserPort = NULL;
            ModifyIDCMP(win, 0);
            Permit();
        }
        CloseWindow(win);
    }
    MGLTerm();

    printf("Demo finished cleanly.\n");
    kprintf("DEMO: Finished cleanly.\n");
    return 0;
}
