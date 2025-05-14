#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <SDL2/SDL.h>

// --- Constantes ---
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define M_PI 3.14159265358979323846

// --- Structures ---
typedef struct {
    float x, y, z;
} Vec3D;

typedef struct {
    float x, y;
} Vec2D;

typedef struct {
    int v1_idx; // Index du premier sommet
    int v2_idx; // Index du deuxième sommet
} Edge;

typedef struct {
    Vec3D* vertices;
    int num_vertices;
    Edge* edges;
    int num_edges;
} Mesh;

typedef struct {
    float m[4][4];
} Mat4x4;

// --- Variables globales pour SDL ---
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;

// --- Fonctions matricielles ---

// Matrice identité
Mat4x4 matrix_identity() {
    Mat4x4 mat = {0};
    mat.m[0][0] = 1.0f;
    mat.m[1][1] = 1.0f;
    mat.m[2][2] = 1.0f;
    mat.m[3][3] = 1.0f;
    return mat;
}

// Multiplication d'un Vec3D par une Mat4x4 (assume w=1 pour l'entrée, retourne Vec3D avec w)
// Le Vec3D de retour stocke x'/w, y'/w, z'/w dans x,y,z et w' dans un champ séparé (ou implicitement)
// Ici, pour simplifier, nous retournons un Vec3D qui stocke x', y', z' et w' (le w est temporaire)
typedef struct { float x, y, z, w; } Vec4D;

Vec4D matrix_multiply_vector(Mat4x4 mat, Vec3D vec) {
    Vec4D result;
    result.x = vec.x * mat.m[0][0] + vec.y * mat.m[1][0] + vec.z * mat.m[2][0] + mat.m[3][0]; // w=1
    result.y = vec.x * mat.m[0][1] + vec.y * mat.m[1][1] + vec.z * mat.m[2][1] + mat.m[3][1];
    result.z = vec.x * mat.m[0][2] + vec.y * mat.m[1][2] + vec.z * mat.m[2][2] + mat.m[3][2];
    result.w = vec.x * mat.m[0][3] + vec.y * mat.m[1][3] + vec.z * mat.m[2][3] + mat.m[3][3];
    return result;
}

// Multiplication de deux matrices 4x4
Mat4x4 matrix_multiply_matrix(Mat4x4 a, Mat4x4 b) {
    Mat4x4 result = {0};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            for (int k = 0; k < 4; ++k) {
                result.m[i][j] += a.m[i][k] * b.m[k][j];
            }
        }
    }
    return result;
}

// Matrice de translation
Mat4x4 matrix_translation(float x, float y, float z) {
    Mat4x4 mat = matrix_identity();
    mat.m[3][0] = x;
    mat.m[3][1] = y;
    mat.m[3][2] = z;
    return mat;
}

// Matrice de rotation autour de l'axe X
Mat4x4 matrix_rotation_x(float angle_rad) {
    Mat4x4 mat = matrix_identity();
    mat.m[1][1] = cosf(angle_rad);
    mat.m[1][2] = sinf(angle_rad);
    mat.m[2][1] = -sinf(angle_rad);
    mat.m[2][2] = cosf(angle_rad);
    return mat;
}

// Matrice de rotation autour de l'axe Y
Mat4x4 matrix_rotation_y(float angle_rad) {
    Mat4x4 mat = matrix_identity();
    mat.m[0][0] = cosf(angle_rad);
    mat.m[0][2] = -sinf(angle_rad); // Correction: signe inversé par rapport à la convention habituelle si z pointe vers nous
    mat.m[2][0] = sinf(angle_rad);
    mat.m[2][2] = cosf(angle_rad);
    return mat;
}

// Matrice de rotation autour de l'axe Z
Mat4x4 matrix_rotation_z(float angle_rad) {
    Mat4x4 mat = matrix_identity();
    mat.m[0][0] = cosf(angle_rad);
    mat.m[0][1] = sinf(angle_rad);
    mat.m[1][0] = -sinf(angle_rad);
    mat.m[1][1] = cosf(angle_rad);
    return mat;
}

// Matrice de projection perspective
Mat4x4 matrix_projection(float fov_deg, float aspect_ratio, float near_plane, float far_plane) {
    Mat4x4 mat = {0};
    float fov_rad = fov_deg * (M_PI / 180.0f);
    float tan_half_fov = tanf(fov_rad / 2.0f);

    mat.m[0][0] = 1.0f / (aspect_ratio * tan_half_fov);
    mat.m[1][1] = 1.0f / tan_half_fov;
    mat.m[2][2] = -(far_plane + near_plane) / (far_plane - near_plane);
    mat.m[2][3] = -1.0f; // Important pour la perspective
    mat.m[3][2] = -(2.0f * far_plane * near_plane) / (far_plane - near_plane);
    mat.m[3][3] = 0.0f;
    return mat;
}

// --- Fonctions Mesh ---
Mesh create_cube_mesh() {
    Mesh cube;
    cube.num_vertices = 8;
    cube.vertices = (Vec3D*)malloc(cube.num_vertices * sizeof(Vec3D));
    // Définition des sommets du cube (centré à l'origine)
    cube.vertices[0] = (Vec3D){-0.5f, -0.5f, -0.5f};
    cube.vertices[1] = (Vec3D){ 0.5f, -0.5f, -0.5f};
    cube.vertices[2] = (Vec3D){ 0.5f,  0.5f, -0.5f};
    cube.vertices[3] = (Vec3D){-0.5f,  0.5f, -0.5f};
    cube.vertices[4] = (Vec3D){-0.5f, -0.5f,  0.5f};
    cube.vertices[5] = (Vec3D){ 0.5f, -0.5f,  0.5f};
    cube.vertices[6] = (Vec3D){ 0.5f,  0.5f,  0.5f};
    cube.vertices[7] = (Vec3D){-0.5f,  0.5f,  0.5f};

    cube.num_edges = 12;
    cube.edges = (Edge*)malloc(cube.num_edges * sizeof(Edge));
    // Face avant
    cube.edges[0] = (Edge){0, 1};
    cube.edges[1] = (Edge){1, 2};
    cube.edges[2] = (Edge){2, 3};
    cube.edges[3] = (Edge){3, 0};
    // Face arrière
    cube.edges[4] = (Edge){4, 5};
    cube.edges[5] = (Edge){5, 6};
    cube.edges[6] = (Edge){6, 7};
    cube.edges[7] = (Edge){7, 4};
    // Liaisons entre faces
    cube.edges[8] = (Edge){0, 4};
    cube.edges[9] = (Edge){1, 5};
    cube.edges[10] = (Edge){2, 6};
    cube.edges[11] = (Edge){3, 7};

    return cube;
}

void free_mesh(Mesh* mesh) {
    if (mesh->vertices) free(mesh->vertices);
    if (mesh->edges) free(mesh->edges);
    mesh->num_vertices = 0;
    mesh->num_edges = 0;
}


// --- Initialisation et Nettoyage SDL ---
bool init_sdl() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL n'a pas pu s'initialiser! Erreur SDL: %s\n", SDL_GetError());
        return false;
    }
    window = SDL_CreateWindow("Minimal 3D Physics Engine",
                                SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (window == NULL) {
        printf("La fenêtre n'a pas pu être créée! Erreur SDL: %s\n", SDL_GetError());
        return false;
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == NULL) {
        printf("Le rendu n'a pas pu être créé! Erreur SDL: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        return false;
    }
    return true;
}

void close_sdl() {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    renderer = NULL;
    window = NULL;
    SDL_Quit();
}

// --- Boucle Principale ---
int main(int argc, char* args[]) {
    if (!init_sdl()) {
        return 1;
    }

    Mesh cube = create_cube_mesh();
    Vec2D projected_points[cube.num_vertices]; // Pour stocker les points projetés

    float angle_x = 0.0f;
    float angle_y = 0.0f;
    float angle_z = 0.0f;

    // Paramètres de la caméra/projection
    float fov = 90.0f;
    float aspect_ratio = (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT;
    float near_plane = 0.1f;
    float far_plane = 100.0f;
    Mat4x4 proj_matrix = matrix_projection(fov, aspect_ratio, near_plane, far_plane);

    // Caméra positionnée en arrière et regardant l'origine
    // Pour un moteur simple, on peut mettre la caméra fixe et translater/roter l'objet
    // Ou on peut bouger la caméra (plus complexe pour une view matrix complète)
    // Ici, on va simplement translater l'objet "en arrière" dans la scène.
    Vec3D camera_pos = {0.0f, 0.0f, -3.0f}; // La caméra est à z=-3

    bool quit = false;
    SDL_Event e;

    Uint32 last_time = SDL_GetTicks();

    while (!quit) {
        Uint32 current_time = SDL_GetTicks();
        float delta_time_s = (current_time - last_time) / 1000.0f;
        last_time = current_time;

        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) {
                quit = true;
            }
        }

        // Mise à jour - Rotation de l'objet
        angle_x += 0.5f * delta_time_s;
        angle_y += 0.8f * delta_time_s;
        // angle_z += 0.3f * delta_time_s; // Moins intéressant pour un cube souvent

        // --- Pipeline de Rendu ---

        // 1. Transformation Modèle (Rotation + Translation pour éloigner de la caméra)
        Mat4x4 rot_x_mat = matrix_rotation_x(angle_x);
        Mat4x4 rot_y_mat = matrix_rotation_y(angle_y);
        Mat4x4 rot_z_mat = matrix_rotation_z(angle_z);
        Mat4x4 trans_mat = matrix_translation(0, 0, camera_pos.z + 2.0f); // Eloigne l'objet pour qu'il soit visible
                                                                       // camera_pos.z est négatif, donc on ajoute une valeur positive
                                                                       // pour le pousser plus loin "dans" l'écran.
                                                                       // Par exemple, si la caméra est à -3, on met l'objet à z = -3 + 2 = -1
                                                                       // Non, on veut le mettre DEVANT la caméra, donc une valeur z positive dans l'espace modèle
                                                                       // qui sera ensuite transformée. Mieux : translater à z=2.0f
                                                                       // et la caméra reste à (0,0,0) dans l'espace vue.
                                                                       // Simplifions: la caméra est à (0,0,0) et regarde vers +Z.
                                                                       // L'objet est translaté à z=2 pour être devant.
        Mat4x4 model_matrix = matrix_identity();
        model_matrix = matrix_multiply_matrix(model_matrix, rot_x_mat);
        model_matrix = matrix_multiply_matrix(model_matrix, rot_y_mat);
        model_matrix = matrix_multiply_matrix(model_matrix, rot_z_mat);
        model_matrix = matrix_multiply_matrix(model_matrix, matrix_translation(0, 0, 2.5f)); // Eloigne le cube de 2.5 unités

        // Pour un moteur simple, la matrice de vue peut être l'identité
        // si la caméra est à l'origine et regarde vers +Z (ou -Z selon la convention).
        // Ici, notre projection suppose que la caméra regarde vers +Z.
        Mat4x4 view_matrix = matrix_identity(); // Caméra à l'origine, regarde vers +Z

        // Matrice Modèle-Vue-Projection (MVP)
        Mat4x4 mv_matrix = matrix_multiply_matrix(view_matrix, model_matrix);
        Mat4x4 mvp_matrix = matrix_multiply_matrix(proj_matrix, mv_matrix);

        // Transformer les sommets
        for (int i = 0; i < cube.num_vertices; ++i) {
            Vec3D current_vertex = cube.vertices[i];
            Vec4D transformed_vertex = matrix_multiply_vector(mvp_matrix, current_vertex);

            // 2. Division Perspective
            if (transformed_vertex.w != 0.0f && transformed_vertex.w > near_plane) { // Simple test de clipping
                transformed_vertex.x /= transformed_vertex.w;
                transformed_vertex.y /= transformed_vertex.w;
                transformed_vertex.z /= transformed_vertex.w; // z n'est pas utilisé pour le dessin 2D, mais utile pour z-buffer

                // 3. Transformation Viewport (NDC vers Coordonnées Écran)
                // NDC x,y sont entre -1 et 1.
                // L'origine (0,0) de l'écran SDL est en haut à gauche.
                projected_points[i].x = (transformed_vertex.x + 1.0f) * 0.5f * SCREEN_WIDTH;
                projected_points[i].y = (1.0f - transformed_vertex.y) * 0.5f * SCREEN_HEIGHT; // Y est inversé
            } else {
                // Point derrière la caméra ou sur le plan proche, on le met hors écran pour éviter les artefacts
                projected_points[i].x = -10000; // Hors écran
                projected_points[i].y = -10000;
            }
        }

        // Rendu
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); // Fond noir
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // Lignes blanches
        for (int i = 0; i < cube.num_edges; ++i) {
            Vec2D p1 = projected_points[cube.edges[i].v1_idx];
            Vec2D p2 = projected_points[cube.edges[i].v2_idx];

            // Ne pas dessiner si l'un des points est "clippé" de manière basique
            if (p1.x > -9000 && p2.x > -9000) {
                 SDL_RenderDrawLine(renderer, (int)p1.x, (int)p1.y, (int)p2.x, (int)p2.y);
            }
        }

        SDL_RenderPresent(renderer);
    }

    free_mesh(&cube);
    close_sdl();
    return 0;
}