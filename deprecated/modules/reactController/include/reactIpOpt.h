/* 
 * Copyright: (C) 2015 iCub Facility - Istituto Italiano di Tecnologia
 * Authors: Ugo Pattacini <ugo.pattacini@iit.it>, Matej Hoffmann <matej.hoffmann@iit.it>, 
 * Alessandro Roncone <alessandro.roncone@yale.edu>
 * website: www.robotcub.org
 * author website: http://alecive.github.io
 * 
 * Permission is granted to copy, distribute, and/or modify this program
 * under the terms of the GNU General Public License, version 2 or any
 * later version published by the Free Software Foundation.
 *
 * A copy of the license can be found at
 * http://www.robotcub.org/icub/license/gpl.txt
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details.
*/

#ifndef __REACTIPOPT_H__
#define __REACTIPOPT_H__

#include <sstream>

#include <IpTNLP.hpp>
#include <IpIpoptApplication.hpp>

#include <yarp/os/all.h>
#include <yarp/dev/all.h>
#include <yarp/sig/all.h>
#include <yarp/math/Math.h>

#include <iCub/ctrl/math.h>
#include <iCub/ctrl/pids.h>
#include <iCub/ctrl/minJerkCtrl.h>
#include <iCub/iKin/iKinFwd.h>
#include <iCub/skinDynLib/common.h>

using namespace std;
using namespace yarp::os;
using namespace yarp::dev;
using namespace yarp::sig;
using namespace yarp::math;
using namespace iCub::ctrl;
using namespace iCub::iKin;

class ControlPoint
{
public:
    string type; //e.g. "elbow"
    yarp::sig::Vector x_desired; //desired Cartesian position (x,y,z) in Root FoR
    yarp::sig::Vector p0; //position of the control point depending on current state of chain
    yarp::sig::Matrix J0_xyz; //Jacobian for position depending on current state of chain

    ControlPoint()
    {
        x_desired.resize(3); x_desired.zero();
        x_desired(0)=-0.2; //just to have it iCub Root FoR friendly
        p0.resize(3); p0.zero();
        p0(0) = -0.1;
        //for J0_xyz we don't know the size yet - depending on the control point
    }

    string toString()
    {
        std::stringstream sstm;
        sstm<< "ControlPoint, type: "<<type<<", x_desired: ("<<x_desired.toString(3,3)<<"), p0: ("<<p0.toString(3,3)<<"), J0_xyz: "<<endl<<
            J0_xyz.toString(3,3)<<endl;

        return sstm.str();
    }
};

/****************************************************************/
class ControllerNLP : public Ipopt::TNLP
{
    iKinChain &chain;
    bool hitting_constraints;
    bool orientation_control;
    bool additional_control_points_flag;

    Vector xr,pr;
    Matrix Hr,skew_nr,skew_sr,skew_ar;
    Matrix q_lim,v_lim;
    Vector q0,v0,v,p0;
    Matrix H0,R0,He,J0_xyz,J0_ang,Derr_ang;
    Vector err_xyz,err_ang;
    Matrix bounds;
    double dt;

    std::vector<ControlPoint> &additional_control_points;
    int extra_ctrl_points_nr;
    double additional_control_points_tol;
    Vector err_xyz_elbow;

    double shou_m,shou_n;
    double elb_m,elb_n;

    Vector qGuard;
    Vector qGuardMinExt;
    Vector qGuardMinInt;
    Vector qGuardMinCOG;
    Vector qGuardMaxExt;
    Vector qGuardMaxInt;
    Vector qGuardMaxCOG;

    /****************************************************************/
    void computeSelfAvoidanceConstraints();
    void computeGuard();
    void computeBounds();
    Matrix v2m(const Vector &x);
    Matrix skew(const Vector &w);

public:
    ControllerNLP(iKinChain &chain_, std::vector<ControlPoint> &additional_control_points_);
    virtual ~ControllerNLP();
    void set_xr(const Vector &xr);
    void set_v_limInDegPerSecond(const Matrix &v_lim);
    void set_hitting_constraints(const bool _hitting_constraints);
    void set_orientation_control(const bool _orientation_control);
    void set_additional_control_points(const bool _additional_control_points_flag);
    void set_dt(const double dt);
    void set_v0InDegPerSecond(const Vector &v0);
    void init();
    Vector get_resultInDegPerSecond() const;
    Property getParameters() const;
    bool get_nlp_info(Ipopt::Index &n, Ipopt::Index &m, Ipopt::Index &nnz_jac_g,
                      Ipopt::Index &nnz_h_lag, IndexStyleEnum &index_style);
    bool get_bounds_info(Ipopt::Index n, Ipopt::Number *x_l, Ipopt::Number *x_u,
                         Ipopt::Index m, Ipopt::Number *g_l, Ipopt::Number *g_u);
    bool get_starting_point(Ipopt::Index n, bool init_x, Ipopt::Number *x,
                            bool init_z, Ipopt::Number *z_L, Ipopt::Number *z_U,
                            Ipopt::Index m, bool init_lambda, Ipopt::Number *lambda);
    void computeQuantities(const Ipopt::Number *x, const bool new_x);
    bool eval_f(Ipopt::Index n, const Ipopt::Number *x, bool new_x, Ipopt::Number &obj_value);
    bool eval_grad_f(Ipopt::Index n, const Ipopt::Number* x, bool new_x, Ipopt::Number *grad_f);
    bool eval_g(Ipopt::Index n, const Ipopt::Number *x, bool new_x,Ipopt::Index m, Ipopt::Number *g);
    bool eval_jac_g(Ipopt::Index n, const Ipopt::Number *x, bool new_x, Ipopt::Index m, Ipopt::Index nele_jac, Ipopt::Index *iRow,
                    Ipopt::Index *jCol, Ipopt::Number *values);
    void finalize_solution(Ipopt::SolverReturn status, Ipopt::Index n, const Ipopt::Number *x, const Ipopt::Number *z_L,
                           const Ipopt::Number *z_U, Ipopt::Index m, const Ipopt::Number *g, const Ipopt::Number *lambda,
                           Ipopt::Number obj_value, const Ipopt::IpoptData *ip_data, Ipopt::IpoptCalculatedQuantities *ip_cq);
};

//#include <sstream>
//
//#include <IpTNLP.hpp>
//#include <IpIpoptApplication.hpp>
//
//#include <iCub/iKin/iKinFwd.h>
//
//#include "common.h"
//
//using namespace std;
//using namespace yarp::os;
//using namespace yarp::dev;
//using namespace yarp::sig;
//using namespace yarp::math;
//using namespace iCub::ctrl;
//using namespace iCub::iKin;
//
//
///****************************************************************/
//class ControllerNLP : public Ipopt::TNLP
//{
//    iCubArm *arm;
//    bool hitting_constraints;
//    bool orientation_control;
//
//    Vector xr,pr, ori_grad, pos_grad;
//    Matrix Hr,skew_nr,skew_sr,skew_ar;
//    Matrix q_lim,v_lim;
//    Vector q0,v0,v,p0, rest_jnt_pos, q1, rest_weights, rest_err;
//    Matrix H0,R0,He,J0_xyz,J0_ang;
//    Vector err_xyz,err_ang;
//    Matrix bounds;
//    double dt, ang_mag, weight, weight2;
//    int chain_dof;
//    int extra_ctrl_points_nr;
//    Vector err_xyz_elbow;
//
//    double shou_m,shou_n;
//    double elb_m,elb_n;
//
//    Vector qGuard;
//    Vector qGuardMinExt;
//    Vector qGuardMinInt;
//    Vector qGuardMinCOG;
//    Vector qGuardMaxExt;
//    Vector qGuardMaxInt;
//    Vector qGuardMaxCOG;
//
//    /****************************************************************/
//    void computeSelfAvoidanceConstraints();
//    void computeGuard();
//    void computeBounds();
//    static Matrix v2m(const Vector &x);
//    static Matrix skew(const Vector &w);
//
//    public:
//    ControllerNLP(iCubArm *arm_, bool hittingConstraints_,
//                  bool orientationControl_, double dT_, double restPosWeight=0.0);
//    ~ControllerNLP() override;
////    void set_xr(const Vector &_xr);
//    void init(const Vector &_xr, const Vector &_v0, const Matrix &_v_lim);
//    Vector get_resultInDegPerSecond() const { return CTRL_RAD2DEG*v; }
//    bool get_nlp_info(Ipopt::Index &n, Ipopt::Index &m, Ipopt::Index &nnz_jac_g,
//                      Ipopt::Index &nnz_h_lag, IndexStyleEnum &index_style) override;
//    bool get_bounds_info(Ipopt::Index n, Ipopt::Number *x_l, Ipopt::Number *x_u,
//                         Ipopt::Index m, Ipopt::Number *g_l, Ipopt::Number *g_u) override;
//    bool get_starting_point(Ipopt::Index n, bool init_x, Ipopt::Number *x,
//                            bool init_z, Ipopt::Number *z_L, Ipopt::Number *z_U,
//                            Ipopt::Index m, bool init_lambda, Ipopt::Number *lambda) override;
//    void computeQuantities(const Ipopt::Number *x, bool new_x);
//    bool eval_f(Ipopt::Index n, const Ipopt::Number *x, bool new_x, Ipopt::Number &obj_value) override;
//    bool eval_grad_f(Ipopt::Index n, const Ipopt::Number* x, bool new_x, Ipopt::Number *grad_f) override;
//    bool eval_g(Ipopt::Index n, const Ipopt::Number *x, bool new_x,Ipopt::Index m, Ipopt::Number *g) override;
//    bool eval_jac_g(Ipopt::Index n, const Ipopt::Number *x, bool new_x, Ipopt::Index m, Ipopt::Index nele_jac, Ipopt::Index *iRow,
//                    Ipopt::Index *jCol, Ipopt::Number *values) override;
//    void finalize_solution(Ipopt::SolverReturn status, Ipopt::Index n, const Ipopt::Number *x, const Ipopt::Number *z_L,
//                           const Ipopt::Number *z_U, Ipopt::Index m, const Ipopt::Number *g, const Ipopt::Number *lambda,
//                           Ipopt::Number obj_value, const Ipopt::IpoptData *ip_data, Ipopt::IpoptCalculatedQuantities *ip_cq) override;
//};


#endif

