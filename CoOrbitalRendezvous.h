#pragma once

#include "Eigen\Dense"
#include "ObjectFiles\ObjectFiles\ObjectFiles.h"
#ifdef COORBIT_EXPORTS
#define COORBIT_API __declspec(dllexport)
#else
#define COORBIT_API __declspec(dllimport)
#endif

using namespace Eigen;

namespace CoOrbital
{
	class Functions
	{
	public:
		static COORBIT_API void CoOrbitalCalculator(Objects::Asset assetObject, Objects::Target targetObject,
			Objects::satCmdMessage& outCommand, double simTime_mjd, double& fuelUsage, double& t_man);

		static COORBIT_API void burn2Calculator(Objects::Asset assetObject, double simTime_mjd,
			Objects::satCmdMessage& outCommand);

		static COORBIT_API void CoOrbitalMinimize(Vector3d r_asset_km, Vector3d dr_asset_kms, double* r_target_km, double* dr_target_kms,
			double t_low, double t_high, Vector3d& dv1, Vector3d& dv2, double& t_man);

	};
}