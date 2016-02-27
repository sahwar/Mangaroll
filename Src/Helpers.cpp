#include "Helpers.h"

using namespace OVR;
namespace OvrMangaroll {
	double Time::Delta = 0;
	double Time::Elapsed = 0;

	const VrFrame *Frame::Current = NULL;

	Vector3f HMD::Direction = Vector3f(0, 0, -1.0f);
	float HMD::HorizontalAngle = 0;
	float HMD::VerticalAngle = 0;

	GuideType AppState::Guide = GuideType::NONE;
}