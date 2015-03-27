#ifndef GLCAMERA_H
#define GLCAMERA_H
/*
Szymon Rusinkiewicz
Princeton University

GLCamera.h
Manages OpenGL camera and trackball/arcball interaction
*/

#include "Vec.h"
#include "XForm.h"
#include "timestamp.h"


namespace trimesh {

namespace Mouse {
	enum button { NONE, ROTATE, MOVEXY, MOVEZ, WHEELUP, WHEELDOWN, LIGHT };
};

class GLCamera {
public:
	enum Constraint { UNCONSTRAINED,
		XCONSTRAINED, YCONSTRAINED, ZCONSTRAINED };

private:
	int lastmousex, lastmousey;
	Mouse::button lastb;
	timestamp last_time;

	vec lightdir;

	bool dospin;
	point spincenter;
	vec spinaxis;
	float spinspeed;

	Constraint constraint_;
	float field_of_view, pixscale;
	mutable float surface_depth;
	float click_depth;
	float tb_screen_x, tb_screen_y, tb_screen_size;
	bool read_depth(int x, int y, point &p) const;

	void startspin();
	vec mouse2tb(float x, float y);
	void rotate(int mousex, int mousey, xform &xf);
	void movexy(int mousex, int mousey, xform &xf);
	void movez(int mousex, int mousey, xform &xf);
	void wheel(Mouse::button updown, xform &xf);
	void relight(int mousex, int mousey);
	void mouse_click(int mousex, int mousey,
			 const point &scene_center, float scene_size);

public:
	GLCamera() : lastb(Mouse::NONE), lightdir(vec(0,0,1)),
		     dospin(false), spinspeed(0),
		     constraint_(UNCONSTRAINED), field_of_view(0.7f),
		     surface_depth(0.0f), click_depth(0.0f)
	{
		lightdir[0] = lightdir[1] = 0; lightdir[2] = 1;
		last_time = now();
	}

	void setupGL(const point &scene_center, float scene_size) const;

	void mouse(int mousex, int mousey, Mouse::button b,
		   const point &scene_center, float scene_size,
		   xform &xf);

	bool autospin(xform &xf);
	void stopspin() { dospin = false; }

	vec light() const { return lightdir; }
	void set_light(const vec &lightdir_) { lightdir = lightdir_; }

	float fov() const { return field_of_view; }
	void set_fov(float fov_) { field_of_view = fov_; }

	Constraint constraint() { return constraint_; }
	void set_constraint(Constraint c) { constraint_ = c; }
};

}; // namespace trimesh

#endif
