/** Copyright (C) 2008-2013, Josh Ventura
*** Copyright (C) 2013-2014, Robert B. Colton
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

#include <stdio.h>
#include "../General/OpenGLHeaders.h"
#include <string.h>
//using std::string;
#include "../General/GStextures.h"
#include "../General/GLTextureStruct.h"
#include "Universal_System/image_formats.h"
#include "Universal_System/backgroundstruct.h"
#include "Universal_System/spritestruct.h"
#include "Graphics_Systems/graphics_mandatory.h"

#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF

vector<TextureStruct*> textureStructs(0);

namespace enigma_user {
extern int room_width, room_height;
}

namespace enigma {
  extern size_t background_idmax;
}

TextureStruct::TextureStruct(unsigned gtex)
{
	gltex = gtex;
}

TextureStruct::~TextureStruct()
{
	glDeleteTextures(1, &gltex);
}

unsigned get_texture(int texid) {
	return (size_t(texid) >= textureStructs.size())? -1 : textureStructs[texid]->gltex;
}

inline unsigned int lgpp2(unsigned int x){//Trailing zero count. lg for perfect powers of two
	x =  (x & -x) - 1;
	x -= ((x >> 1) & 0x55555555);
	x =  ((x >> 2) & 0x33333333) + (x & 0x33333333);
	x =  ((x >> 4) + x) & 0x0f0f0f0f;
	x += x >> 8;
	return (x + (x >> 16)) & 63;
}

namespace enigma
{
  int graphics_create_texture(unsigned width, unsigned height, unsigned fullwidth, unsigned fullheight, void* pxdata, bool mipmap)
  {
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, 4, fullwidth, fullheight, 0, GL_BGRA, GL_UNSIGNED_BYTE, pxdata);
    // This allows us to control the number of mipmaps generated, but Direct3D does not have an option for it, so for now we'll just go with the defaults.
    // Honestly not a big deal, Unity3D doesn't allow you to specify either.
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 2);
    // The user should call texture_mipmapping_generate manually to control when mipmaps are generated.
    glBindTexture(GL_TEXTURE_2D, 0);

    TextureStruct* textureStruct = new TextureStruct(texture);
    textureStruct->width = width;
    textureStruct->height = height;
    textureStruct->fullwidth = fullwidth;
    textureStruct->fullheight = fullheight;
    textureStructs.push_back(textureStruct);
    return textureStructs.size()-1;
  }

  int graphics_duplicate_texture(int tex, bool mipmap)
  {
    GLuint texture = textureStructs[tex]->gltex;
    glBindTexture(GL_TEXTURE_2D, texture);
    unsigned w, h, fw, fh;
    w = textureStructs[tex]->width;
    h = textureStructs[tex]->height;
    fw = textureStructs[tex]->fullwidth;
    fh = textureStructs[tex]->fullheight;
    char* bitmap = new char[(fh<<(lgpp2(fw)+2))|2];
    glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, bitmap);
    unsigned dup_tex = graphics_create_texture(w, h, fw, fh, bitmap, mipmap);
    delete[] bitmap;
    glPopAttrib();
    return dup_tex;
  }

  void graphics_replace_texture_alpha_from_texture(int tex, int copy_tex)
  {
    GLuint texture = textureStructs[tex]->gltex;
    GLuint copy_texture = textureStructs[copy_tex]->gltex;

    unsigned w, h, fw, fh, size;
    glBindTexture(GL_TEXTURE_2D, texture);
    w = textureStructs[tex]->width;
    h = textureStructs[tex]->height;
    fw = textureStructs[tex]->fullwidth;
    fh = textureStructs[tex]->fullheight;
    size = (fh<<(lgpp2(fw)+2))|2;
    char* bitmap = new char[size];
    char* bitmap2 = new char[size];
    glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, bitmap);
    glBindTexture(GL_TEXTURE_2D, copy_texture);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, bitmap2);

    for (int i = 3; i < size; i += 4)
        bitmap[i] = (bitmap2[i-3] + bitmap2[i-2] + bitmap2[i-1])/3;

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, 4, fw, fh, 0, GL_BGRA, GL_UNSIGNED_BYTE, bitmap);

    glBindTexture(GL_TEXTURE_2D, 0);

    delete[] bitmap;
    delete[] bitmap2;
    glPopAttrib();
  }

  void graphics_delete_texture(int texid)
  {
    delete textureStructs[texid];
  }

  unsigned char* graphics_get_texture_pixeldata(unsigned texture, unsigned* fullwidth, unsigned* fullheight)
  {
    enigma_user::texture_set(texture);

    *fullwidth = textureStructs[texture]->fullwidth;
    *fullheight = textureStructs[texture]->fullheight;

    unsigned char* ret = new unsigned char[((*fullwidth)*(*fullheight)*4)];
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, ret);

    return ret;
  }
  
  //NOTE: OpenGL 1 hardware does not support sampler objects, some versions of 2 and usually over 3 do. We use this class
  //to emulate the Direct3D behavior.
  struct SamplerState {
    unsigned bound_texture;
    bool wrapu, wrapv, wrapw;
    GLint bordercolor[4], min, mag, maxlevel;
    GLfloat anisotropy, minlod, maxlod;
    
    SamplerState(): wrapu(true), wrapv(true), wrapw(true) {
    
    }
    
    ~SamplerState() {
    
    }
    
    void ApplyWrap() {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, wrapu?GL_REPEAT:GL_CLAMP);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapv?GL_REPEAT:GL_CLAMP);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapw?GL_REPEAT:GL_CLAMP); 
    }
    
    void ApplyAnisotropy() {
      glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy);
    }
    
    void ApplyBorderColor() {
      glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, bordercolor);
    }
    
    void ApplyFilter() {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
    }
    
    void ApplyLod() {
     // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_LOD, minlod);
     // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, maxlod);
      //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, maxlevel);
    }
    
    void ApplyState() {
      ApplyWrap();
      ApplyAnisotropy();
      ApplyFilter();
      ApplyBorderColor();
      ApplyLod();
    }
  };
  
  SamplerState samplerstates[8];
}

namespace enigma_user
{

int texture_add(string filename, bool mipmap) {
  unsigned int w, h, fullwidth, fullheight;

  unsigned char *pxdata = enigma::image_load(filename,&w,&h,&fullwidth,&fullheight,false);
  if (pxdata == NULL) { printf("ERROR - Failed to append sprite to index!\n"); return -1; }
  unsigned texture = enigma::graphics_create_texture(w, h, fullwidth, fullheight, pxdata, mipmap);
  delete[] pxdata;
    
  return texture;
}

void texture_save(int texid, string fname) {
	unsigned w, h;
	unsigned char* rgbdata = enigma::graphics_get_texture_pixeldata(texid, &w, &h);

  string ext = enigma::image_get_format(fname);

	enigma::image_save(fname, rgbdata, w, h, w, h, false);

	delete[] rgbdata;
}

void texture_delete(int texid) {
  delete textureStructs[texid];
}

bool texture_exists(int texid) {
  return textureStructs[texid] != NULL;
}

void texture_preload(int texid)
{
  // Deprecated in ENIGMA and GM: Studio, all textures are automatically preloaded.
}

void texture_set_priority(int texid, double prio)
{
  // Deprecated in ENIGMA and GM: Studio, all textures are automatically preloaded.
  glBindTexture(GL_TEXTURE_2D, textureStructs[texid]->gltex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, prio);
}

void texture_set_enabled(bool enable)
{
  (enable?glEnable:glDisable)(GL_TEXTURE_2D);
}

void texture_set_blending(bool enable)
{
    (enable?glEnable:glDisable)(GL_BLEND);
}

gs_scalar texture_get_width(int texid) {
	return textureStructs[texid]->width / textureStructs[texid]->fullwidth;
}

gs_scalar texture_get_height(int texid)
{
	return textureStructs[texid]->height / textureStructs[texid]->fullheight;
}

unsigned texture_get_texel_width(int texid)
{
	return textureStructs[texid]->width;
}

unsigned texture_get_texel_height(int texid)
{
	return textureStructs[texid]->height;
}

void texture_set(int texid) {
  texture_set_stage(0, texid);
}

void texture_set_stage(int stage, int texid) {
  if (enigma::samplerstates[stage].bound_texture != unsigned(get_texture(texid))) {
    glActiveTexture(GL_TEXTURE0 + stage);
    glBindTexture(GL_TEXTURE_2D, enigma::samplerstates[stage].bound_texture = get_texture(texid));
    enigma::samplerstates[stage].ApplyState();
  }
}

void texture_reset() {
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, enigma::samplerstates[0].bound_texture = 0);
  enigma::samplerstates[0].ApplyState();
}

void texture_set_interpolation(bool enable) {
  texture_set_interpolation_ext(0, enable);
}

void texture_set_interpolation_ext(int sampler, bool enable)
{
  glActiveTexture(GL_TEXTURE0 + sampler);
  enigma::samplerstates[sampler].min = enable?GL_LINEAR:GL_NEAREST;
  enigma::samplerstates[sampler].mag = enable?GL_LINEAR:GL_NEAREST;
  enigma::samplerstates[sampler].ApplyFilter();
}

void texture_set_repeat(bool repeat) {
  texture_set_repeat_ext(0, repeat);
}

void texture_set_repeat_ext(int sampler, bool repeat)
{
  glActiveTexture(GL_TEXTURE0 + sampler);
  enigma::samplerstates[sampler].wrapu = repeat;
  enigma::samplerstates[sampler].wrapu = repeat;
  enigma::samplerstates[sampler].wrapu = repeat;
  enigma::samplerstates[sampler].ApplyWrap();
}

void texture_set_wrap_ext(int sampler, bool wrapu, bool wrapv, bool wrapw)
{
  glActiveTexture(GL_TEXTURE0 + sampler);
  enigma::samplerstates[sampler].wrapu = wrapu;
  enigma::samplerstates[sampler].wrapu = wrapv;
  enigma::samplerstates[sampler].wrapu = wrapw;
  enigma::samplerstates[sampler].ApplyWrap();
}

void texture_set_border_ext(int sampler, int r, int g, int b, double a)
{
  enigma::samplerstates[sampler].bordercolor[0] = r;
  enigma::samplerstates[sampler].bordercolor[1] = g;
  enigma::samplerstates[sampler].bordercolor[2] = b;
  enigma::samplerstates[sampler].bordercolor[3] = int(a);
  glActiveTexture(GL_TEXTURE0 + sampler);
  enigma::samplerstates[sampler].ApplyBorderColor();
}

void texture_set_filter_ext(int sampler, int filter)
{
  glActiveTexture(GL_TEXTURE0 + sampler);
  if (filter == tx_trilinear) {
    enigma::samplerstates[sampler].min = GL_LINEAR_MIPMAP_LINEAR;
    enigma::samplerstates[sampler].mag = GL_LINEAR;
  } else if (filter == tx_bilinear) {
    enigma::samplerstates[sampler].min = GL_LINEAR_MIPMAP_NEAREST;
    enigma::samplerstates[sampler].mag = GL_LINEAR;
  } else if (filter == tx_nearest) {
    enigma::samplerstates[sampler].min = GL_NEAREST_MIPMAP_NEAREST;
    enigma::samplerstates[sampler].mag = GL_NEAREST;
  } else {
    enigma::samplerstates[sampler].min = GL_NEAREST;
    enigma::samplerstates[sampler].mag = GL_NEAREST;
  }
  enigma::samplerstates[sampler].ApplyFilter();
}

void texture_set_lod_ext(int sampler, double minlod, double maxlod, int maxlevel)
{
  glActiveTexture(GL_TEXTURE0 + sampler);
  enigma::samplerstates[sampler].minlod = minlod;
  enigma::samplerstates[sampler].maxlod = maxlod;
  enigma::samplerstates[sampler].maxlevel = maxlevel;
  enigma::samplerstates[sampler].ApplyLod();
}

bool texture_mipmapping_supported()
{
  return strstr((char*)glGetString(GL_EXTENSIONS),
           "glGenerateMipmap");
}

void texture_mipmapping_generate(int texid)
{
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, enigma::samplerstates[0].bound_texture = textureStructs[texid]->gltex);
  // This allows us to control the number of mipmaps generated, but Direct3D does not have an option for it, so for now we'll just go with the defaults.
  // Honestly not a big deal, Unity3D doesn't allow you to specify either.
  //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
  //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 3);
  glGenerateMipmap(GL_TEXTURE_2D);
}

bool texture_anisotropy_supported()
{
  return strstr((char*)glGetString(GL_EXTENSIONS),
           "GL_EXT_texture_filter_anisotropic");
}

float texture_anisotropy_maxlevel()
{
  float maximumAnisotropy;
  glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maximumAnisotropy);
  return maximumAnisotropy;
}

void  texture_anisotropy_filter(int sampler, gs_scalar levels)
{
  glActiveTexture(GL_TEXTURE0 + sampler);
  enigma::samplerstates[sampler].anisotropy = levels;
  enigma::samplerstates[sampler].ApplyState();
}

}
