// CoOrbitalRendezvous.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include <cmath>
#include "Propagator\Propagator\Propagator.h"
#include "CoOrbitalRendezvous.h"
#include <iostream>

using namespace Eigen;

const double mu = 398600.4418;
const double pi = 3.14159265359;

double BurnEstimator(double dryMass_kg, double fuel_kg, double Isp_s, double massRate_kgs, double deltaV_ms)
{
	double mf, m0;
	const double g0 = 9.80665;
	m0 = dryMass_kg + fuel_kg;
	mf = m0 / log(deltaV_ms / (g0*Isp_s));
	return (m0 - mf) / massRate_kgs;
}

void Ric2Eci(Vector3d r_chief, Vector3d dr_chief, Vector3d& r_deputy, Vector3d& dr_deputy, Vector3d r_HCW, Vector3d dr_HCW)
{
	/*
	Algorithm converts a deputy state in the relative frame to a state in the inertial frame
	*/

	Vector3d h, rci_X, rci_Xd, rci_Y, rci_Yd, rci_Z, rci_Zd;
	Matrix3d C, Cd;
	h = r_chief.cross(dr_chief);

	rci_X = r_chief / r_chief.norm();
	rci_Z = h / h.norm();
	rci_Y = rci_Z.cross(rci_X);

	rci_Xd = (1 / r_chief.norm())*(dr_chief - rci_X.dot(dr_chief)*rci_X);
	rci_Zd << 0, 0, 0;
	rci_Yd = rci_Z.cross(rci_Xd);

	C << rci_X(0), rci_X(1), rci_X(2),
		rci_Y(0), rci_Y(1), rci_Y(2),
		rci_Z(0), rci_Z(1), rci_Z(2);
	Cd << rci_Xd(0), rci_Xd(1), rci_Xd(2),
		rci_Yd(0), rci_Yd(1), rci_Yd(2),
		rci_Zd(0), rci_Zd(1), rci_Zd(2);

	//cout << "C\n" << C << "\nCd\n" << Cd << endl;

	r_deputy = C.transpose()*r_HCW;
	dr_deputy = Cd.transpose()*r_HCW + C.transpose()*dr_HCW;

	r_deputy += r_chief;
	dr_deputy += dr_chief;

}

void tof(double *t, double p, double m, double el, double k, double *f, double *g, double *fdot, double *gdot, double *sindE,
	double *a, double cosdNu, double sindNu, double r1, double r2, double Nrev)
{
	*a = m*k*p / ((2 * m - el*el)*p*p + 2 * k*el*p - k*k);
	*f = 1 - r2*(1 - cosdNu) / p;
	*g = r1*r2*sindNu / sqrt(mu*p);
	*fdot = sqrt(mu / p)*((1 - cosdNu) / sindNu)*((1 - cosdNu) / p - 1 / r1 - 1 / r2);
	*gdot = 1 - r1 / p*(1 - cosdNu);
	double cosdE = 1 - r1 / (*a)*(1 - *f);
	*sindE = -r1*r2*(*fdot) / sqrt(mu*(*a));
	double dE = acos(cosdE);
	if (*sindE < 0)
	{
		dE = 2 * pi - dE;
	}

	if (dE < 0)
	{
		dE = dE + 2 * pi;
	}
	else if (dE >(2 * pi))
	{
		dE = dE - 2 * pi;
	}
	*t = *g + sqrt((*a)*(*a)*(*a) / mu)*(2 * pi*Nrev + dE - *sindE);
}

double iterateP(double t, double tman, double p, double sindE, double g, double a, double k, double el, double m)
{
	double dtdp = -g / (2 * p) - 1.5*a*(t - g)*((k*k + (2 * m - el*el)*p*p) / m / k / p / p) +
		sqrt(a*a*a / mu) * 2 * k*sindE / p / (k - el*p);
	return (p - (t - tman) / dtdp);
}

bool longTest(Vector3d r_Asset, Vector3d r_Target)
{
	Matrix2d deter_M;
	deter_M << r_Asset(0), r_Asset(1),
		r_Target(0), r_Target(1);
	if (atan2(deter_M.determinant(), r_Asset.dot(r_Target)) < 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool P_iter(Vector3d r_asset, Vector3d r_target, double t_des, int N_rev,
	Vector3d& v1, Vector3d& v2)
{
	/*
	Function calculates the intersect velocity for solving Lambert's Problem using a p-iteration teqnique as
	specified in:

	Bate, Mueller, and White, "Fundamentals of Astrodynamics". Dover. 1971.

	Code written by Capt Perry VanZandt, Nov 2017.

	Code assisted from Capt Barry Witt with his thesis "Mission Planning for Close-Proximity Satellites", 2008.

	Tested Successfully 20 Jan 18
	*/
	
	double t, p, f, g, fdot, gdot, sindE, a;
	int iter = 0;;
	// Set default value of exit flag

	double r1 = r_asset.norm();
	
	double r2 = r_target.norm();
	double cosdNu = r_asset.dot(r_target) / r1 / r2;
	double sindNu = sqrt(1 - cosdNu*cosdNu);
	double k = r1*r2*(1 - cosdNu);
	double el = r1 + r2;
	double m = r1*r2*(1 + cosdNu);
	double p_i = k / (el + sqrt(2 * m));
	double p_ii = k / (el - sqrt(2 * m));

	// Test if asset needs to go long or short ways around orbit
	if (longTest(r_asset, r_target))
	{
		sindNu = -sindNu;
		p = p_ii - 0.0001;
	}
	else
	{
		p = p_i + 0.0001;
	}

	t = 0;

	while ((abs(t - t_des) > (1e-6)) && (iter < 100))
	{
		tof(&t, p, m, el, k, &f, &g, &fdot, &gdot, &sindE, &a, cosdNu, sindNu, r1, r2, N_rev);
		if ((a < 0) || (p < 0))
		{
			return false;
		}

		p = iterateP(t, t_des, p, sindE, g, a, k, el, m);
		//std::cout << "p: " << p << std::endl;
		//system("pause");
		
	}

	if (iter == 100)
	{
		return false;
	}

	v1 = (r_target - f*r_asset) / g;
	v2 = (gdot*r_target - r_asset) / g;

	// Set exit flag to true to indicate successful solution
	return true;
}


void CoOrbital::Functions::CoOrbitalMinimize(Vector3d r_asset_km, Vector3d dr_asset_kms, double* r_target_km, double* dr_target_kms,
	double t_low, double t_high, Vector3d& dv1, Vector3d& dv2, double& t_man)
{
	/*
		Function designed to find a time of flight to approach the minimum for a transfer between a chaser satellite
		and a target satellite. Tested 20 Jan 18 successfully.
	*/
	
	std::cout << "Starting minimization\n";
	double min, minT, t_step, t_max, dV, offset;
	Vector3d r, dr, v1, v2, mindv1, mindv2;
	int N;
	t_step = 3600;
	min = 1e6;
	minT = 0;
	offset = 20;
	t_max = t_high;

	for (int kk = 0; kk < 10; kk++)
	{
		std::cout << kk << std::endl;
		for (double t = t_low; t < t_high; t = t + t_step)
		{
			N = (int)floor((t - 10000) / 86400);
			//std::cout << r_target_km[0] << std::endl << r_target_km[1] << std::endl << r_target_km[2] << std::endl;
			//std::cout << dr_target_kms[0] << std::endl << dr_target_kms[1] << std::endl << dr_target_kms[2] << t << std::endl;
			Propagator::Functions::Propagate(r_target_km, dr_target_kms, t, r, dr);

			if (longTest(r_asset_km, r))
			{
				Vector3d rHCW, vHCW, rch, vch;
				rch = r; vch = dr;
				rHCW << 0, -offset, 0;
				vHCW << 0, 0, 0;
				Ric2Eci(rch, vch, r, dr, rHCW, vHCW);
			}
			else
			{
				Vector3d rHCW, vHCW, rch, vch;
				rch = r; vch = dr;
				rHCW << 0, offset, 0;
				vHCW << 0, 0, 0;
				Ric2Eci(rch, vch, r, dr, rHCW, vHCW);
			}

			if (!P_iter(r_asset_km, r, t, N, v1, v2))
			{
				continue;
			}
			//std::cout << v1 << std::endl << v1 << std::endl;

			dV = (v1 - dr_asset_kms).norm() + (v2 - dr).norm();
			//std::cout << dV << std::endl;
			if (dV < min)
			{
				min = dV;
				minT = t;
				std::cout << "New Min dV: " << min * 1000 << " m/s\nt: " << minT << " s\n";
				//std::cout << "Target Position:\n" << r << std::endl;
				mindv1 = v1 - dr_asset_kms;
				mindv2 = dr - v2;
			}

		}
		t_low = minT - 2 * t_step;
		t_high = minT + 2 * t_step;
		if (t_high > t_max)
		{
			t_high = t_max;
		}
		t_step /= 2;
	}
	dv1 = mindv1;
	dv2 = mindv2;
	t_man = minT;
}


void CoOrbital::Functions::CoOrbitalCalculator(Objects::Asset assetObject, Objects::Target targetObject, 
	Objects::satCmdMessage& outCommand, double simTime_mjd, double& fuelUsage, double& t_man)
{
	Vector3d dv1, dv2, r_asset_km, dr_asset_kms;
	// Assume for testing both objects are at same epoch (1 hr past sim time)
	for (int kk = 0; kk < 3; kk++)
	{
		r_asset_km(kk) = assetObject.oscState_pos_gcrf_km[kk];
		dr_asset_kms(kk) = assetObject.oscState_vel_gcrf_kms[kk];
	}
	std::cout << r_asset_km << std::endl;
	std::cout << dr_asset_kms << std::endl;
	double t_low = 86400 * (targetObject.manWindow_mjd[0] - simTime_mjd);
	double t_high = 86400 * (targetObject.manWindow_mjd[1] - simTime_mjd);
	
	CoOrbitalMinimize(r_asset_km, dr_asset_kms, targetObject.oscState_pos_gcrf_km, targetObject.oscState_vel_gcrf_kms,
		t_low, t_high, dv1, dv2, t_man);


	outCommand.durationEstimate_s = BurnEstimator(assetObject.dryMass_kg, assetObject.fuelRemaining_kg, assetObject.Isp_s,
		assetObject.massRate_kgs, dv1.norm());
	outCommand.startTime_mjd = simTime_mjd + 0.04166667 - outCommand.durationEstimate_s / 2 / 86400;
	for (int kk = 0; kk < 3; kk++)
	{
		outCommand.thrustVector_kms[kk] = dv1(kk);
	}

	outCommand.type = New;
	outCommand.referenceFrame = gcrf;

	// Estimate total fuel usage for rendezvous
	fuelUsage = (outCommand.durationEstimate_s + BurnEstimator(assetObject.dryMass_kg, assetObject.fuelRemaining_kg, assetObject.Isp_s,
		assetObject.massRate_kgs, dv2.norm()))*assetObject.massRate_kgs;
}

void CoOrbital::Functions::burn2Calculator(Objects::Asset assetObject, double simTime_mjd, 
	Objects::satCmdMessage& outCommand)
{
	Vector3d r_asset, v_asset, r_target, v_target, dV;

	// Write function within co-orbital rendezvous concerning calcluation of second burn

	
	// Propoagte both asset and target to rendezvous time
	Propagator::Functions::Propagate(assetObject.oscState_pos_gcrf_km, assetObject.oscState_vel_gcrf_kms,
		assetObject.stateMachineObject->coastStart_mjd + assetObject.stateMachineObject->coastTime_mjd - assetObject.epoch_mjd,
		r_asset, v_asset);


	Propagator::Functions::Propagate(assetObject.stateMachineObject->target.oscState_pos_gcrf_km,
		assetObject.stateMachineObject->target.oscState_vel_gcrf_kms, assetObject.stateMachineObject->coastStart_mjd +
		assetObject.stateMachineObject->coastTime_mjd - assetObject.stateMachineObject->target.epoch_mjd,
		r_target, v_target);

	// Zero velocity difference
	dV = v_target - v_asset;

	// Formulate Command for second burn
	outCommand.durationEstimate_s = BurnEstimator(assetObject.dryMass_kg, assetObject.fuelRemaining_kg, assetObject.Isp_s,
		assetObject.massRate_kgs, dV.norm());

	outCommand.startTime_mjd = assetObject.stateMachineObject->coastStart_mjd + assetObject.stateMachineObject->coastTime_mjd -
		outCommand.durationEstimate_s / 2 / 86400;

	for (int kk = 0; kk < 3; kk++)
	{
		outCommand.thrustVector_kms[kk] = dV(kk);
	}

	outCommand.type = New;
	outCommand.referenceFrame = gcrf;
}