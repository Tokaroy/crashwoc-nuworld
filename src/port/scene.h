#ifndef NUWORLD_PORT_SCENE_H
#define NUWORLD_PORT_SCENE_H

#include <stdbool.h>
#include <stdint.h>

/* World space; yaw 0 and pitch 0 look along +z. */
typedef struct NwSceneCam {
    float pos[3], yaw, pitch;
} NwSceneCam;

typedef struct NwSceneInfo {
    NwSceneCam cam;
    float speed;    /* a quarter of the instances' spread a second */
    uint32_t objects;
    uint32_t instances;
    uint64_t tris;
    uint32_t seq;
    bool drawn;
} NwSceneInfo;

/* Game thread entry. */
int nw_scene_main(int argc, char **argv);
/* Any thread: false until the first load. */
bool nw_scene_info(NwSceneInfo *info);
void nw_scene_set_camera(const NwSceneCam *cam);
/* Any thread: a disc path (either slash), loaded in place of the current scene. */
void nw_scene_load(const char *path);

#endif
