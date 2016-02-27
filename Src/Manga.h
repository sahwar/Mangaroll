#pragma once

#include "App.h"
#include "GlObject.h"
#include "Page.h"

using namespace OVR;

namespace OvrMangaroll {

	class Manga : public GlObject
	{
	public:
		Manga(void);
		~Manga(void);
		void Draw(const Matrix4f&);
		void AddPage(Page *page);
		void Update(float angle);
	private:
		GlProgram _Prog;
		int _Count;
		Page *_First;
		Page *_Last;
		Page *_Selection;
	};

}