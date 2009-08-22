/*********************************************************************************\
**                                                                              **
**  Copyright (C) 2008 Josh Ventura                                             **
**                                                                              **
**  This file is a part of the ENIGMA Development Environment.                  **
**                                                                              **
**                                                                              **
**  ENIGMA is free software: you can redistribute it and/or modify it under the **
**  terms of the GNU General Public License as published by the Free Software   **
**  Foundation, version 3 of the license or any later version.                  **
**                                                                              **
**  This application and its source code is distributed AS-IS, WITHOUT ANY      **
**  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS   **
**  FOR A PARTICULAR PURPOSE. See the GNU General Public License for more       **
**  details.                                                                    **
**                                                                              **
**  You should have recieved a copy of the GNU General Public License along     **
**  with this code. If not, see <http://www.gnu.org/licenses/>                  **
**                                                                              **
**  ENIGMA is an environment designed to create games and other programs with a **
**  high-level, fully compilable language. Developers of ENIGMA or anything     **
**  associated with ENIGMA are in no way responsible for its users or           **
**  applications created by its users, or damages caused by the environment     **
**  or programs made in the environment.                                        **                      
**                                                                              **
\*********************************************************************************/

/*****************************************************

int room_goto(double roomind)
int room_restart()
int room_goto_absolute(double index)
int room_goto_first()
int room_goto_next()
int room_next(double num)
int room_previous(double num)

******************************************************/


namespace enigma
{
  struct inst
  {
    int x, y, id, obj;
  };
  struct roomstruct
  {
    std::string name;
    int backcolor;
    
    void(*createcode)();
    
    std::string cap;
    int width, height, spd;
    
    char views_enabled,start_vis[8];
    int area_x[8],area_y[8],area_w[8],area_h[8];
    int port_x[8],port_y[8],port_w[8],port_h[8];
    int object2follow[8];
    int hborder[8],vborder[8],hspd[8],vspd[8];
    
    int instancecount;
    std::map<int,inst> instances;
    
    void gotome();
  };
  
  int room_max;
  std::map<int,roomstruct> roomdata;
  std::map<int,enigma::roomstruct>::iterator roomiter;
}

int room_goto(double roomind);
int room_restart();
int room_goto_absolute(double index);
int room_goto_first();
int room_goto_next();
int room_next(double num);
int room_previous(double num);

















/*
Source
*/


namespace enigma
{
   void roomstruct::gotome()
   {
      room_caption=cap;
      room_width=width;
      room_height=height;
      room_speed=spd;
      background_color=backcolor;
      background_showcolor=(backcolor!=-1);
      
      view_enabled=views_enabled;
      for(int i=0;i<7;i++) 
      {
        view_xview[i]=area_x[i]; view_yview[i]=area_y[i]; view_wview[i]=area_w[i]; view_hview[i]=area_h[i];
        view_xport[i]=port_x[i]; view_yport[i]=port_y[i]; view_wport[i]=port_w[i]; view_hport[i]=port_h[i];
        view_object[i]=object2follow[i];
        view_hborder[i]=hborder[i]; view_vborder[i]=vborder[i]; view_hspeed[i]=hspd[i]; view_vspeed[i]=vspd[i];
        view_visible[i]=start_vis[i];
      }
      
       double xm=0,ym=0;
       
       if (view_enabled) for (int i=0;i<7;i++) 
       {
          if (view_visible[i]) 
          { if (view_xview[i]+view_wview[i]>xm) xm=(int)(view_xview[i]+view_wview[i]); 
            if (view_yview[i]+view_hview[i]>ym) ym=(int)(view_yview[i]+view_hview[i]); } 
       } else { xm=room_width; ym=room_height; }
       
       window_set_size(xm,ym);
      
      for (int i=0; i<instancecount; i++)
      {
        inst obj=instances[i];
        
        int a=obj.id;
        if (a>enigma::maxid) enigma::maxid=a;
        
        instance_create_id(obj.x,obj.y,obj.obj,a);
      }
      
      createcode();
  }
}

int room_goto(double roomind)
{
    int indx=(int)roomind;
    
    if (enigma::roomdata.find(indx)==enigma::roomdata.end())
      {
        #if SHOWERRORS
        show_error("Attempting to go to nonexisting room",0);
        #endif
        return 0;
      } //error like GM here
    
    //Destroy all objects
    enigma::nodestroy=1;
    for (enigma::instance_iterator=enigma::instance_list.begin(); enigma::instance_iterator != enigma::instance_list.end(); enigma::instance_iterator++)
    {
      (*enigma::instance_iterator).second->myevent_roomend();
      #ifdef ISCONTROLLER_persistent
      if (!(*enigma::instance_iterator).second->persistent)
      #endif
      instance_destroy((*enigma::instance_iterator).second->id);
    }
    enigma::nodestroy=0;
    
    room.realval=indx;
    enigma::roomdata[indx].gotome();
    
    return 0;
}

int room_restart()
{
    int indx=(int)room.realval;
    
    if (enigma::roomdata.find(indx)==enigma::roomdata.end())
      {
        #if SHOWERRORS
        show_error("Is this some kind of joke? <__<",0);
        #endif
        return 0;
      } //error like GM here
    
    //Destroy all objects
    enigma::nodestroy=1;
    for (enigma::instance_iterator=enigma::instance_list.begin(); enigma::instance_iterator != enigma::instance_list.end(); enigma::instance_iterator++)
    {
      (*enigma::instance_iterator).second->myevent_roomend();
      #ifdef ISCONTROLLER_persistent
      if (!(*enigma::instance_iterator).second->persistent)
      #endif
      instance_destroy((*enigma::instance_iterator).second->id);
    }
    enigma::nodestroy=0;
    
    enigma::roomdata[indx].gotome();
    
    return 0;
}

int room_goto_absolute(double index)
{
    enigma::roomiter=enigma::roomdata.begin();
    //for (int ii=0; ii<(int)index; ii++) if (enigma::roomiter==enigma::roomdata.end()) { break; } else enigma::roomiter++;
    
    if (enigma::roomiter==enigma::roomdata.end())
    {
        #if SHOWERRORS
        if (enigma::roomdata.empty())
        show_error("Game must have at least one room to run",0);
        else show_error("Attempting to go to nonexisting room",0);
        #endif
        return 0;
    }
    
    int indx=(*enigma::roomiter).first;
    
    //Destroy all objects
    enigma::nodestroy=1;
    for (enigma::instance_iterator=enigma::instance_list.begin(); enigma::instance_iterator != enigma::instance_list.end(); enigma::instance_iterator++)
    {
      (*enigma::instance_iterator).second->myevent_roomend();
      #ifdef ISCONTROLLER_persistent
      if (!(*enigma::instance_iterator).second->persistent)
      #endif
      instance_destroy((*enigma::instance_iterator).second->id);
    }
    enigma::nodestroy=0;
    
    room.realval=indx;
    enigma::roomdata[indx].gotome();
    
    return 0;
}



int room_goto_first()
{
    enigma::roomiter=enigma::roomdata.begin();
    if (enigma::roomiter==enigma::roomdata.end())
    {
        #if SHOWERRORS
        show_error("Game must have at least one room to run",0);
        #endif
        return 0;
    }
    
    int indx=(*enigma::roomiter).first;
    
    //Destroy all objects
    enigma::nodestroy=1;
    for (enigma::instance_iterator=enigma::instance_list.begin(); enigma::instance_iterator != enigma::instance_list.end(); enigma::instance_iterator++)
    {
      (*enigma::instance_iterator).second->myevent_roomend();
      #ifdef ISCONTROLLER_persistent
      if (!(*enigma::instance_iterator).second->persistent)
      #endif
      instance_destroy((*enigma::instance_iterator).second->id);
    }
    enigma::nodestroy=0;
    
    room.realval=indx;
    enigma::roomdata[indx].gotome();
    
    return 0;
}


int room_goto_next()
{
    for (enigma::roomiter=enigma::roomdata.begin();enigma::roomiter!=enigma::roomdata.end();enigma::roomiter++)
    if ((*enigma::roomiter).first==(int)room) break;
    
    if (enigma::roomiter==enigma::roomdata.end())
      {
        #if SHOWERRORS
        show_error("Going to next room from unexisting room (?)",0);
        #endif
        return 0;
      } //error like GM here
      
    enigma::roomiter++;
    
    if (enigma::roomiter==enigma::roomdata.end())
      {
        #if SHOWERRORS
        show_error("Going to next room after last",0);
        #endif
        return 0;
      } //error like GM here
    
    //Destroy all objects
    enigma::nodestroy=1;
    for (enigma::instance_iterator=enigma::instance_list.begin(); enigma::instance_iterator != enigma::instance_list.end(); enigma::instance_iterator++)
    {
      (*enigma::instance_iterator).second->myevent_roomend();
      #ifdef ISCONTROLLER_persistent
      if (!(*enigma::instance_iterator).second->persistent)
      #endif
      instance_destroy((*enigma::instance_iterator).second->id);
    }
    enigma::nodestroy=0;
    
    room.realval=(*enigma::roomiter).first;
    (*enigma::roomiter).second.gotome();
    return 0;
}


int room_next(double num)
{
    enigma::roomiter=enigma::roomdata.find((int)num);
    if (enigma::roomiter==enigma::roomdata.end()) { show_error("Retrieving next room after nonexisting room "+string(num),0); return -1; }
    enigma::roomiter++; if (enigma::roomiter==enigma::roomdata.end()) { return -1; }
    return (*enigma::roomiter).first;
}
int room_previous(double num)
{
    enigma::roomiter=enigma::roomdata.find((int)num);
    if (enigma::roomiter==enigma::roomdata.end()) { show_error("Retrieving previous room before nonexisting room "+string(num),0); return -1; }
    enigma::roomiter--; if (enigma::roomiter==enigma::roomdata.end()) { return -1; }
    return (*enigma::roomiter).first;
}

