/** Copyright (C) 2008-2013 polygone
***
*** This file is a part of the ENIGMA Development Environment.
***
*** ENIGMA is free software: you can redistribute it and/or modify it under the
*** terms of the GNU General Public License as published by the Free Software
*** Foundation, version 3 of the license or any later version.
***
*** This application and its source code is distributed AS-IS, WITHOUT ANY
*** WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
*** FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
*** details.
***
*** You should have received a copy of the GNU General Public License along
*** with this code. If not, see <http://www.gnu.org/licenses/>
**/

// Tile system
#include "Universal_System/depth_draw.h"
#include <algorithm>
#include "../General/GSbackground.h"
#include "Universal_System/backgroundstruct.h"
#include "../General/GStextures.h"
#include "GL3TextureStruct.h"
#include "../General/GStiles.h"
#include "../General/GLtilestruct.h"
#include "../General/OpenGLHeaders.h"
#include "../General/GSprimitives.h" //pr_trianglestrip
#include "../General/GSmodel.h" //For batcher

#ifdef DEBUG_MODE
  #include <string>
  #include "libEGMstd.h"
  #include "Widget_Systems/widgets_mandatory.h"
  #define get_background(bck2d,back)\
    if (back < 0 or size_t(back) >= enigma::background_idmax or !enigma::backgroundstructarray[back]) {\
      show_error("Attempting to draw non-existing background " + toString(back), false);\
      return;\
    }\
    const enigma::background *const bck2d = enigma::backgroundstructarray[back];
#else
  #define get_background(bck2d,back)\
    const enigma::background *const bck2d = enigma::backgroundstructarray[back];
#endif

#define __GETR(x) ((x & 0x0000FF))
#define __GETG(x) ((x & 0x00FF00) >> 8)
#define __GETB(x) ((x & 0xFF0000) >> 16)

namespace enigma
{
    static void draw_tile(int index, int back, gs_scalar left, gs_scalar top, gs_scalar width, gs_scalar height, gs_scalar x, gs_scalar y, gs_scalar xscale, gs_scalar yscale, int color, double alpha)
    {
        if (!enigma_user::background_exists(back)) return;
        get_background(bck2d,back);
        float tbw = bck2d->width/(float)bck2d->texbordx, tbh = bck2d->height/(float)bck2d->texbordy,
              xvert1 = x, xvert2 = xvert1 + width*xscale,
              yvert1 = y, yvert2 = yvert1 + height*yscale,
              tbx1 = left/tbw, tbx2 = tbx1 + width/tbw,
              tby1 = top/tbh, tby2 = tby1 + height/tbh;

        //TODO: The model should probably be populated manually along with indicies. The _end() calls a lot of useless code now. Upside is that this needs to be done once.
        enigma_user::d3d_model_primitive_begin(index, enigma_user::pr_trianglestrip);
        enigma_user::d3d_model_vertex_texture_color(index, xvert1, yvert1, tbx1, tby1, color, alpha);
        enigma_user::d3d_model_vertex_texture_color(index, xvert2, yvert1, tbx2, tby1, color, alpha);
        enigma_user::d3d_model_vertex_texture_color(index, xvert1, yvert2, tbx1, tby2, color, alpha);
        enigma_user::d3d_model_vertex_texture_color(index, xvert2, yvert2, tbx2, tby2, color, alpha);
        enigma_user::d3d_model_primitive_end(index);
    }

    void load_tiles()
    {
        int prev_bkid;
        int vert_size = 0;
        int vert_start = 0;
        for (enigma::diter dit = drawing_depths.rbegin(); dit != drawing_depths.rend(); dit++){
            if (dit->second.tiles.size())
            {
                //TODO: Should they really be sorted by background? This may help batching, but breaks compatiblity. Nothing texture atlas wouldn't solve.
                sort(dit->second.tiles.begin(), dit->second.tiles.end(), bkinxcomp);
                int index = enigma_user::d3d_model_create(false);
                drawing_depths[dit->second.tiles[0].depth].tilelist = index;
                vert_size = 0;
                vert_start = 0;
                for(std::vector<tile>::size_type i = 0; i != dit->second.tiles.size(); ++i)
                {
                    tile t = dit->second.tiles[i];
                    if (i==0){ prev_bkid = t.bckid; }
                    draw_tile(index, t.bckid, t.bgx, t.bgy, t.width, t.height, t.roomX, t.roomY, t.xscale, t.yscale, t.color, t.alpha);

                    if (prev_bkid != t.bckid || i == dit->second.tiles.size()-1){ //Texture switch has happened. Create new batch
                        get_background(bck2d,prev_bkid);
                        drawing_depths[dit->second.tiles[0].depth].tilevector.push_back( vector< int >(3) );
                        drawing_depths[dit->second.tiles[0].depth].tilevector.back()[0] = textureStructs[bck2d->texture]->gltex;
                        drawing_depths[dit->second.tiles[0].depth].tilevector.back()[1] = vert_start;
                        drawing_depths[dit->second.tiles[0].depth].tilevector.back()[2] = vert_size;
                        //printf("Texture id = %i and vertices to render = %i and start = %i\n", prev_bkid, vert_size, vert_start );
                        vert_start += vert_size;
                        vert_size = 0;
                        //printf("Texture switch at i = %i and now texture = %i\n", i, prev_bkid );
                        prev_bkid = t.bckid;
                    }
                    vert_size+=6;
                    //printf("Tile = %i, tile x = %i and tile y = %i\n", i, t.bgx, t.bgy);
                }
                drawing_depths[dit->second.tiles[0].depth].tilevector.back()[2] += 6; //Add last quad
            }
        }
    }

    void delete_tiles()
    {
        for (enigma::diter dit = drawing_depths.rbegin(); dit != drawing_depths.rend(); dit++){
            if (dit->second.tiles.size()){
                drawing_depths[dit->second.tiles[0].depth].tilevector.clear();
                enigma_user::d3d_model_destroy( drawing_depths[dit->second.tiles[0].depth].tilelist );
            }
        }
    }

    void rebuild_tile_layer(int layer_depth)
    {
        int prev_bkid;
        for (enigma::diter dit = drawing_depths.rbegin(); dit != drawing_depths.rend(); dit++){
            if (dit->second.tiles.size())
            {
                if (dit->second.tiles[0].depth != layer_depth)
                    continue;

                //TODO: Should they really be sorted by background? This may help batching, but breaks compatiblity. Nothing texture atlas wouldn't solve.
                //sort(dit->second.tiles.begin(), dit->second.tiles.end(), bkinxcomp);
                int index = drawing_depths[dit->second.tiles[0].depth].tilelist;
                if (enigma_user::d3d_model_exists( index )){
                    enigma_user::d3d_model_clear( index );
                    drawing_depths[dit->second.tiles[0].depth].tilevector.clear();
                }else{
                    index = enigma_user::d3d_model_create(false);
                    drawing_depths[dit->second.tiles[0].depth].tilelist = index;
                }
                int vert_size = 0;
                int vert_start = 0;
                for(std::vector<tile>::size_type i = 0; i != dit->second.tiles.size(); ++i)
                {
                    tile t = dit->second.tiles[i];
                    if (i==0){ prev_bkid = t.bckid; }
                    draw_tile(index, t.bckid, t.bgx, t.bgy, t.width, t.height, t.roomX, t.roomY, t.xscale, t.yscale, t.color, t.alpha);

                    if (prev_bkid != t.bckid || i == dit->second.tiles.size()-1){ //Texture switch has happened. Create new batch
                        get_background(bck2d,prev_bkid);
                        drawing_depths[dit->second.tiles[0].depth].tilevector.push_back( vector< int >(3) );
                        drawing_depths[dit->second.tiles[0].depth].tilevector.back()[0] = textureStructs[bck2d->texture]->gltex;
                        drawing_depths[dit->second.tiles[0].depth].tilevector.back()[1] = vert_start;
                        drawing_depths[dit->second.tiles[0].depth].tilevector.back()[2] = vert_size;
                        //printf("Texture id = %i and vertices to render = %i and start = %i\n", prev_bkid, vert_size, vert_start );
                        vert_start += vert_size;
                        vert_size = 0;
                        //printf("Texture switch at i = %i and now texture = %i\n", i, prev_bkid );
                        prev_bkid = t.bckid;
                    }
                    vert_size+=6;
                    //printf("Tile = %i, vertices = %i, prev_bkid = %i, t.bckid = %i, size() = %i\n", i, vert_size,prev_bkid, t.bckid, dit->second.tiles.size() );
                }
                drawing_depths[dit->second.tiles[0].depth].tilevector.back()[2] += 6; //Add last quad
                break;
            }
        }
    }
}

namespace enigma_user
{

int tile_add(int background, int left, int top, int width, int height, int x, int y, int depth, double xscale, double yscale, double alpha, int color)
{
    enigma::tile *ntile = new enigma::tile;
    ntile->id = enigma::maxtileid++;
    ntile->bckid = background;
    ntile->bgx = left;
    ntile->bgy = top;
    ntile->width = width;
    ntile->height = height;
    ntile->roomX = x;
    ntile->roomY = y;
    ntile->depth = depth;
    ntile->alpha = alpha;
    ntile->color = color;
    ntile->xscale = xscale;
    ntile->yscale = yscale;
    enigma::drawing_depths[ntile->depth].tiles.push_back(*ntile);
    enigma::rebuild_tile_layer(ntile->depth);
    return ntile->id;
}

bool tile_delete(int id)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
            {
                enigma::tile t = dit->second.tiles[i];
                if (t.id == id)
                {
                    enigma::drawing_depths[t.depth].tiles.erase(enigma::drawing_depths[t.depth].tiles.begin() + i);
                    enigma::rebuild_tile_layer(t.depth);
                    return true;
                }
            }
    return false;
}

bool tile_exists(int id)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
                if (dit->second.tiles[i].id == id)
                    return true;
    return false;
}

double tile_get_alpha(int id)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
                if (dit->second.tiles[i].id == id)
                    return dit->second.tiles[i].alpha;
    return 0;
}

int tile_get_background(int id)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
                if (dit->second.tiles[i].id == id)
                    return dit->second.tiles[i].bckid;
    return 0;
}

int tile_get_blend(int id)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
                if (dit->second.tiles[i].id == id)
                    return dit->second.tiles[i].color;
    return 0;
}

int tile_get_depth(int id)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
                if (dit->second.tiles[i].id == id)
                    return dit->second.tiles[i].depth;
    return 0;
}

int tile_get_height(int id)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
                if (dit->second.tiles[i].id == id)
                    return dit->second.tiles[i].height;
    return 0;
}

int tile_get_left(int id)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
                if (dit->second.tiles[i].id == id)
                    return dit->second.tiles[i].bgx;
    return 0;
}

int tile_get_top(int id)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
                if (dit->second.tiles[i].id == id)
                    return dit->second.tiles[i].bgy;
    return 0;
}

double tile_get_visible(int id)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
                if (dit->second.tiles[i].id == id)
                    return (dit->second.tiles[i].alpha > 0);
    return 0;
}

bool tile_get_width(int id)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
                if (dit->second.tiles[i].id == id)
                    return dit->second.tiles[i].width;
    return 0;
}

int tile_get_x(int id)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
                if (dit->second.tiles[i].id == id)
                    return dit->second.tiles[i].roomX;
    return 0;
}

int tile_get_xscale(int id)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
                if (dit->second.tiles[i].id == id)
                    return dit->second.tiles[i].xscale;
    return 0;
}

int tile_get_y(int id)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
                if (dit->second.tiles[i].id == id)
                    return dit->second.tiles[i].roomY;
    return 0;
}

int tile_get_yscale(int id)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
                if (dit->second.tiles[i].id == id)
                    return dit->second.tiles[i].yscale;
    return 0;
}

bool tile_set_alpha(int id, double alpha)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
                if (dit->second.tiles[i].id == id)
                {
                    dit->second.tiles[i].alpha = alpha;
                    enigma::rebuild_tile_layer(dit->second.tiles[i].depth);
                    return true;
                }
    return false;
}

bool tile_set_background(int id, int background)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
                if (dit->second.tiles[i].id == id)
                {
                    dit->second.tiles[i].bckid = background;
                    enigma::rebuild_tile_layer(dit->second.tiles[i].depth);
                    return true;
                }
    return false;
}

bool tile_set_blend(int id, int color)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
                if (dit->second.tiles[i].id == id)
                {
                    dit->second.tiles[i].color = color;
                    enigma::rebuild_tile_layer(dit->second.tiles[i].depth);
                    return true;
                }
    return false;
}

bool tile_set_position(int id, int x, int y)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
                if (dit->second.tiles[i].id == id)
                {
                    dit->second.tiles[i].roomX = x;
                    dit->second.tiles[i].roomY = y;
                    enigma::rebuild_tile_layer(dit->second.tiles[i].depth);
                    return true;
                }
    return false;
}

bool tile_set_region(int id, int left, int top, int width, int height)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
                if (dit->second.tiles[i].id == id)
                {
                    dit->second.tiles[i].bgx = left;
                    dit->second.tiles[i].bgy = top;
                    dit->second.tiles[i].width = width;
                    dit->second.tiles[i].height = height;
                    enigma::rebuild_tile_layer(dit->second.tiles[i].depth);
                    return true;
                }
    return false;
}

bool tile_set_scale(int id, int xscale, int yscale)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
                if (dit->second.tiles[i].id == id)
                {
                    dit->second.tiles[i].xscale = xscale;
                    dit->second.tiles[i].yscale = yscale;
                    enigma::rebuild_tile_layer(dit->second.tiles[i].depth);
                    return true;
                }
    return false;
}

bool tile_set_visible(int id, bool visible)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
                if (dit->second.tiles[i].id == id)
                {
                    dit->second.tiles[i].alpha = visible?1:0;
                    enigma::rebuild_tile_layer(dit->second.tiles[i].depth);
                    return true;
                }
    return false;
}

bool tile_set_depth(int id, int depth)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
            {
                enigma::tile t = dit->second.tiles[i];
                if (t.id == id)
                {
                    enigma::drawing_depths[t.depth].tiles.erase(enigma::drawing_depths[t.depth].tiles.begin() + i);
                    enigma::rebuild_tile_layer(t.depth);
                    t.depth = depth;
                    enigma::drawing_depths[t.depth].tiles.push_back(t);
                    enigma::rebuild_tile_layer(t.depth);
                    return true;
                }
            }
    return false;
}

bool tile_layer_delete(int layer_depth)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
        {
            if (dit->second.tiles[0].depth != layer_depth)
                continue;
            glDeleteLists(enigma::drawing_depths[dit->second.tiles[0].depth].tilelist, 1);
            dit->second.tiles.clear();
            return true;
        }
    return false;
}

bool tile_layer_delete_at(int layer_depth, int x, int y)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
        {
            if (dit->second.tiles[0].depth != layer_depth)
                continue;
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
            {
                enigma::tile t = dit->second.tiles[i];
                if (t.roomX == x && t.roomY == y)
                    enigma::drawing_depths[t.depth].tiles.erase(enigma::drawing_depths[t.depth].tiles.begin() + i);
            }
            enigma::rebuild_tile_layer(layer_depth);
            return true;
        }
    return false;
}

bool tile_layer_depth(int layer_depth, int depth)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
        {
            if (dit->second.tiles[0].depth != layer_depth)
                continue;
            enigma::drawing_depths[depth].tilelist = enigma::drawing_depths[layer_depth].tilelist;
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
            {
                enigma::tile t = dit->second.tiles[i];
                t.depth = depth;
                enigma::drawing_depths[t.depth].tiles.push_back(t);
            }
            enigma::drawing_depths[layer_depth].tiles.clear();
            enigma::rebuild_tile_layer(depth);
            return true;
        }
    return false;
}

int tile_layer_find(int layer_depth, int x, int y)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
        {
            if (dit->second.tiles[0].depth != layer_depth)
                continue;
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
            {
                enigma::tile t = dit->second.tiles[i];
                if (t.roomX == x && t.roomY == y)
                    return t.id;
            }
        }
    return -1;
}

bool tile_layer_hide(int layer_depth)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
        {
            if (dit->second.tiles[0].depth != layer_depth)
                continue;
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
            {
                enigma::tile &t = dit->second.tiles[i];
                t.alpha = 0;
            }
            return true;
        }
    return false;
}

bool tile_layer_show(int layer_depth)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
        {
            if (dit->second.tiles[0].depth != layer_depth)
                continue;
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
            {
                enigma::tile &t = dit->second.tiles[i];
                t.alpha = 1;
            }
            return true;
        }
    return false;
}

bool tile_layer_shift(int layer_depth, int x, int y)
{
    for (enigma::diter dit = enigma::drawing_depths.rbegin(); dit != enigma::drawing_depths.rend(); dit++)
        if (dit->second.tiles.size())
        {
            if (dit->second.tiles[0].depth != layer_depth)
                continue;
            for(std::vector<enigma::tile>::size_type i = 0; i !=  dit->second.tiles.size(); i++)
            {
                enigma::tile &t = dit->second.tiles[i];
                t.roomX += x;
                t.roomY += y;
            }
            return true;
        }
    return false;
}

}

