#ifndef NUWORLD_PORT_NUGSCN_H
#define NUWORLD_PORT_NUGSCN_H

struct nugscn_s;

/* A scene the game drops without this is freed when a later load overwrites its heap range. */
void nw_gscn_remove(struct nugscn_s *scene);

#endif
