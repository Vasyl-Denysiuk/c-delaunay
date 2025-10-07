#include <stddef.h>
#include <stdlib.h>
#include <pthread.h>

#include <raylib.h>
#include <rlgl.h>
#include <raymath.h>

typedef struct lcell lcell;
typedef struct llist llist;
typedef struct point point;
typedef struct q_edge q_edge;

void delaunay(point* points, size_t count);
q_edge* sym(q_edge* e);

struct point {
    float x;
    float y;
};

struct q_edge {
    point* org;
    q_edge* nxt;
    q_edge* rot;
};

struct lcell {
    point* p1;
    point* p2;
    lcell* nxt;
};

Vector2 conv(point* p) {
    return (Vector2) {p->x, p->y};
}

point inv_conv(Vector2 p) {
    return (point) {p.x, p.y};
}

extern lcell* head;

point points[64];
size_t  points_cnt = 0;

pthread_mutex_t child_active_mtx;
pthread_cond_t child_continue;

int terminate_child = 0;

void* child_thrd_func(void* _) {
    pthread_mutex_lock(&child_active_mtx);

    delaunay(points, points_cnt);

    pthread_mutex_unlock(&child_active_mtx);
    return 0;
}

int main() {
    int child_spawned = 0;
    pthread_t child_thr = { 0 };

    pthread_mutex_init(&child_active_mtx, NULL);
    pthread_cond_init(&child_continue, NULL);

    const int screenWidth = 800;
    const int screenHeight = 800;

    SetConfigFlags(FLAG_MSAA_4X_HINT);

    InitWindow(screenWidth, screenHeight, "Delaunay");
    SetTargetFPS(60);

    Camera2D camera = { 0 };
    camera.zoom = 1.0f;

    while (!WindowShouldClose()) {

        if (IsKeyPressed(KEY_A)) {
            Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), camera);
            points[points_cnt] = inv_conv(mouseWorldPos);
            points_cnt++;

            if (points_cnt >= 64) { exit(1); }
        } else if (IsKeyPressed(KEY_ENTER)) {
            if (!child_spawned) {
                pthread_create(&child_thr, NULL, child_thrd_func, NULL);
                child_spawned = 1;
            }
        } else if (IsKeyPressed(KEY_SPACE)) {
            pthread_cond_signal(&child_continue);
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 delta = GetMouseDelta();
            delta = Vector2Scale(delta, -1.0f/camera.zoom);
            camera.target = Vector2Add(camera.target, delta);
        }

        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), camera);

            camera.offset = GetMousePosition();
            camera.target = mouseWorldPos;

            float scale = 0.2f*wheel;

            camera.zoom = Clamp(camera.zoom*expf(scale), 0.125f, 64.0f);
        }
        Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), camera);
        BeginDrawing();

        ClearBackground(RAYWHITE);

        BeginMode2D(camera);

            rlPushMatrix();
            rlTranslatef(0, 25*50, 0);
            rlRotatef(90, 1, 0, 0);
            DrawGrid(100, 50);
            rlPopMatrix();

            pthread_mutex_lock(&child_active_mtx);

            for (size_t i=0; i < points_cnt; i++) {
                DrawCircleV(conv(&points[i]), 4, BLACK);
            }

            lcell* curr = head;
            while (curr != NULL) {
                DrawLineEx(conv(curr->p1), conv(curr->p2), 4, RED);
                curr = curr->nxt;
            };

            pthread_mutex_unlock(&child_active_mtx);

        EndMode2D();

        DrawCircleV(GetWorldToScreen2D((Vector2){0, 0}, camera), 5, MAROON);

        DrawCircleV(GetMousePosition(), 3, DARKGRAY);
        DrawText(TextFormat("(%.2f, %.2f)", mouseWorldPos.x, mouseWorldPos.y), GetMouseX()-44, GetMouseY()-24, 20, BLACK);

        EndDrawing();
    }

    CloseWindow();

    terminate_child = 1;
    pthread_cond_broadcast(&child_continue);

    if (child_spawned) pthread_join(child_thr, NULL);

    return 0;
}
