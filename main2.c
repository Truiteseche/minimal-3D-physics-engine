#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// --- Configuration ---
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define CUBE_SIZE 1.0f // Taille du côté du cube

// --- Structures ---
typedef struct {
    float x, y, z;
} Vec3D;

typedef struct {
    int v1_idx; // Index du premier sommet
    int v2_idx; // Index du second sommet
} Edge;

typedef struct {
    Vec3D* vertices;
    int num_vertices;
    Edge* edges;
    int num_edges;
    Vec3D position;
    Vec3D rotation; // Angles d'Euler (en radians)
    Vec3D scale;
} Object3D;

typedef struct {
    float m[4][4];
} Mat4x4;

// --- Variables Globales ---
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
Object3D cube;
Mat4x4 mat_proj;
float fov_degrees = 90.0f;
float aspect_ratio = (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT;
float near_plane = 0.1f;
float far_plane = 100.0f;

// --- Fonctions Matricielles ---

// Matrice Identité
Mat4x4 matrix_identity() {
    Mat4x4 mat = {0};
    mat.m[0][0] = 1.0f;
    mat.m[1][1] = 1.0f;
    mat.m[2][2] = 1.0f;
    mat.m[3][3] = 1.0f;
    return mat;
}

// Multiplication Vecteur * Matrice (avec gestion de w pour la projection)
// Le vecteur d'entrée est traité comme (v.x, v.y, v.z, 1.0)
void multiply_matrix_vector(Vec3D in, Vec3D* out, float* w_out, Mat4x4 mat) {
    out->x = in.x * mat.m[0][0] + in.y * mat.m[1][0] + in.z * mat.m[2][0] + mat.m[3][0];
    out->y = in.x * mat.m[0][1] + in.y * mat.m[1][1] + in.z * mat.m[2][1] + mat.m[3][1];
    out->z = in.x * mat.m[0][2] + in.y * mat.m[1][2] + in.z * mat.m[2][2] + mat.m[3][2];
    *w_out = in.x * mat.m[0][3] + in.y * mat.m[1][3] + in.z * mat.m[2][3] + mat.m[3][3];
}

// Multiplication Matrice * Matrice
Mat4x4 matrix_multiply_matrix(Mat4x4 a, Mat4x4 b) {
    Mat4x4 result = {0};
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            result.m[r][c] = a.m[r][0] * b.m[0][c] +
                             a.m[r][1] * b.m[1][c] +
                             a.m[r][2] * b.m[2][c] +
                             a.m[r][3] * b.m[3][c];
        }
    }
    return result;
}

// Matrice de Translation
Mat4x4 matrix_make_translation(float x, float y, float z) {
    Mat4x4 mat = matrix_identity();
    mat.m[3][0] = x;
    mat.m[3][1] = y;
    mat.m[3][2] = z;
    return mat;
}

// Matrice de Rotation sur X
Mat4x4 matrix_make_rotation_x(float angle_rad) {
    Mat4x4 mat = matrix_identity();
    mat.m[1][1] = cosf(angle_rad);
    mat.m[1][2] = sinf(angle_rad);
    mat.m[2][1] = -sinf(angle_rad);
    mat.m[2][2] = cosf(angle_rad);
    return mat;
}

// Matrice de Rotation sur Y
Mat4x4 matrix_make_rotation_y(float angle_rad) {
    Mat4x4 mat = matrix_identity();
    mat.m[0][0] = cosf(angle_rad);
    mat.m[0][2] = sinf(angle_rad);
    mat.m[2][0] = -sinf(angle_rad);
    mat.m[2][2] = cosf(angle_rad);
    return mat;
}

// Matrice de Rotation sur Z
Mat4x4 matrix_make_rotation_z(float angle_rad) {
    Mat4x4 mat = matrix_identity();
    mat.m[0][0] = cosf(angle_rad);
    mat.m[0][1] = sinf(angle_rad);
    mat.m[1][0] = -sinf(angle_rad);
    mat.m[1][1] = cosf(angle_rad);
    return mat;
}

// Matrice de Mise à l'échelle
Mat4x4 matrix_make_scale(float x, float y, float z) {
    Mat4x4 mat = matrix_identity();
    mat.m[0][0] = x;
    mat.m[1][1] = y;
    mat.m[2][2] = z;
    return mat;
}

// Matrice de Projection Perspective
Mat4x4 matrix_make_projection(float fov_deg, float aspect_ratio, float near, float far) {
    Mat4x4 mat = {0};
    float fov_rad = 1.0f / tanf(fov_deg * 0.5f * (M_PI / 180.0f));
    mat.m[0][0] = aspect_ratio * fov_rad;
    mat.m[1][1] = fov_rad;
    mat.m[2][2] = far / (far - near);
    mat.m[3][2] = (-far * near) / (far - near);
    mat.m[2][3] = 1.0f; // Pour mettre z dans w
    mat.m[3][3] = 0.0f;
    return mat;
}


// --- Fonctions Utilitaires ---
void create_cube(Object3D* obj, float size) {
    obj->num_vertices = 8;
    obj->vertices = (Vec3D*)malloc(obj->num_vertices * sizeof(Vec3D));
    float s = size / 2.0f;
    // Sommets du cube (centré à l'origine)
    obj->vertices[0] = (Vec3D){-s, -s, -s};
    obj->vertices[1] = (Vec3D){ s, -s, -s};
    obj->vertices[2] = (Vec3D){ s,  s, -s};
    obj->vertices[3] = (Vec3D){-s,  s, -s};
    obj->vertices[4] = (Vec3D){-s, -s,  s};
    obj->vertices[5] = (Vec3D){ s, -s,  s};
    obj->vertices[6] = (Vec3D){ s,  s,  s};
    obj->vertices[7] = (Vec3D){-s,  s,  s};

    obj->num_edges = 12;
    obj->edges = (Edge*)malloc(obj->num_edges * sizeof(Edge));
    // Arêtes du cube
    obj->edges[0]  = (Edge){0, 1}; obj->edges[1]  = (Edge){1, 2};
    obj->edges[2]  = (Edge){2, 3}; obj->edges[3]  = (Edge){3, 0};
    obj->edges[4]  = (Edge){4, 5}; obj->edges[5]  = (Edge){5, 6};
    obj->edges[6]  = (Edge){6, 7}; obj->edges[7]  = (Edge){7, 4};
    obj->edges[8]  = (Edge){0, 4}; obj->edges[9]  = (Edge){1, 5};
    obj->edges[10] = (Edge){2, 6}; obj->edges[11] = (Edge){3, 7};

    obj->position = (Vec3D){0.0f, 0.0f, 0.0f}; // Positionné à l'origine
    obj->rotation = (Vec3D){0.0f, 0.0f, 0.0f}; // Pas de rotation initiale
    obj->scale    = (Vec3D){1.0f, 1.0f, 1.0f}; // Echelle unité
}

void free_object(Object3D* obj) {
    if (obj->vertices) free(obj->vertices);
    if (obj->edges) free(obj->edges);
}

// --- Initialisation et Nettoyage SDL ---
int init_sdl() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL n'a pas pu s'initialiser! SDL_Error: %s\n", SDL_GetError());
        return 0;
    }
    window = SDL_CreateWindow("Moteur 3D Filaire Simple",
                              SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        printf("La fenêtre n'a pas pu être créée! SDL_Error: %s\n", SDL_GetError());
        return 0;
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        printf("Le rendu n'a pas pu être créé! SDL_Error: %s\n", SDL_GetError());
        return 0;
    }
    return 1;
}

void close_sdl() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    renderer = NULL;
    window = NULL;
    SDL_Quit();
}

// --- Boucle Principale ---
int main(int argc, char* argv[]) {
    if (!init_sdl()) {
        return 1;
    }

    create_cube(&cube, CUBE_SIZE);
    cube.position.z = 3.0f; // Eloigne le cube de la caméra pour le voir

    // Créer la matrice de projection une seule fois (ou si le FOV/aspect change)
    mat_proj = matrix_make_projection(fov_degrees, aspect_ratio, near_plane, far_plane);

    int quit = 0;
    SDL_Event e;
    Uint32 last_time = SDL_GetTicks();

    while (!quit) {
        Uint32 current_time = SDL_GetTicks();
        // float delta_time = (current_time - last_time) / 1000.0f; // Pas utilisé ici, mais utile pour animation basée sur le temps
        last_time = current_time;

        // Gestion des événements
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = 1;
            }
            if (e.type == SDL_KEYDOWN) {
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: quit = 1; break;
                    case SDLK_UP:    cube.rotation.x -= 0.1f; break;
                    case SDLK_DOWN:  cube.rotation.x += 0.1f; break;
                    case SDLK_LEFT:  cube.rotation.y -= 0.1f; break;
                    case SDLK_RIGHT: cube.rotation.y += 0.1f; break;
                }
            }
        }

        // Préparer les matrices de transformation
        Mat4x4 mat_rot_x = matrix_make_rotation_x(cube.rotation.x);
        Mat4x4 mat_rot_y = matrix_make_rotation_y(cube.rotation.y);
        Mat4x4 mat_rot_z = matrix_make_rotation_z(cube.rotation.z);
        Mat4x4 mat_trans = matrix_make_translation(cube.position.x, cube.position.y, cube.position.z);
        Mat4x4 mat_scale = matrix_make_scale(cube.scale.x, cube.scale.y, cube.scale.z);

        // Matrice Monde (Model Matrix): Scale -> Rotate -> Translate
        Mat4x4 mat_world = matrix_identity();
        mat_world = matrix_multiply_matrix(mat_scale, mat_world); // Ou initialiser mat_world avec mat_scale
        mat_world = matrix_multiply_matrix(mat_rot_x, mat_world); // Appliquer rotations
        mat_world = matrix_multiply_matrix(mat_rot_y, mat_world);
        mat_world = matrix_multiply_matrix(mat_rot_z, mat_world);
        mat_world = matrix_multiply_matrix(mat_trans, mat_world); // Puis translation

        // Rendu
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Fond noir
        SDL_RenderClear(renderer);

        Vec3D transformed_vertices[cube.num_vertices];

        // Transformer tous les sommets
        for (int i = 0; i < cube.num_vertices; ++i) {
            Vec3D v_world, v_projected;
            float w;

            // 1. Transformation Monde (Model vers World space)
            // La fonction multiply_matrix_vector ne prend pas w en entrée, on le suppose à 1
            // Pour la multiplication par mat_world, w_out n'est pas critique ici.
            float dummy_w;
            multiply_matrix_vector(cube.vertices[i], &v_world, &dummy_w, mat_world);

            // 2. Transformation Projection (World vers Screen space)
            multiply_matrix_vector(v_world, &v_projected, &w, mat_proj);

            // 3. Division Perspective (si w != 0 et point devant la caméra)
            if (w != 0.0f /*&& w > near_plane*/) { // Le w de la projection est la distance, donc > near_plane
                v_projected.x /= w;
                v_projected.y /= w;
                v_projected.z /= w; // z normalisé peut être utilisé pour le Z-buffering
            } else { // Point derrière la caméra ou sur le plan w=0, on le met loin
                v_projected.x = 100000; v_projected.y = 100000;
            }


            // 4. Transformation Viewport (NDC vers Coordonnées écran)
            // Les coordonnées normalisées (NDC) sont entre -1 et 1.
            // On les mappe à l'écran (0,0) en haut à gauche.
            transformed_vertices[i].x = (v_projected.x + 1.0f) * 0.5f * SCREEN_WIDTH;
            transformed_vertices[i].y = (1.0f - v_projected.y) * 0.5f * SCREEN_HEIGHT; // Y inversé
            transformed_vertices[i].z = v_projected.z; // Garder le z pour un éventuel Z-buffer
        }

        // Dessiner les arêtes
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // Lignes blanches
        for (int i = 0; i < cube.num_edges; ++i) {
            Vec3D v1_screen = transformed_vertices[cube.edges[i].v1_idx];
            Vec3D v2_screen = transformed_vertices[cube.edges[i].v2_idx];

            // Simple "clipping" : ne dessine pas si les points sont trop loin
            // (ceci est une approximation grossière, un vrai clipping est plus complexe)
            // SDL gère de toute façon les coordonnées hors écran.
            SDL_RenderDrawLine(renderer,
                               (int)v1_screen.x, (int)v1_screen.y,
                               (int)v2_screen.x, (int)v2_screen.y);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16); // Viser ~60 FPS
    }

    free_object(&cube);
    close_sdl();
    return 0;
}