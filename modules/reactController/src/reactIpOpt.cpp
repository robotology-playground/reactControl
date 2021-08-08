/* 
 * Copyright: (C) 2015 iCub Facility - Istituto Italiano di Tecnologia
 * Authors: Ugo Pattacini <ugo.pattacini@iit.it>, Matej Hoffmann <matej.hoffmann@iit.it>, 
 * Alessandro Roncone <alessandro.roncone@yale.edu>
 * website: www.robotcub.org
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

#include "reactIpOpt.h"

    /****************************************************************/
    void ControllerNLP::computeSelfAvoidanceConstraints()
    {
        double joint1_0, joint1_1;
        double joint2_0, joint2_1;
        joint1_0= 28.0*CTRL_DEG2RAD;
        joint1_1= 23.0*CTRL_DEG2RAD;
        joint2_0=-37.0*CTRL_DEG2RAD;
        joint2_1= 80.0*CTRL_DEG2RAD;
        shou_m=(joint1_1-joint1_0)/(joint2_1-joint2_0);
        shou_n=joint1_0-shou_m*joint2_0;

        double joint3_0, joint3_1;
        double joint4_0, joint4_1;
        joint3_0= 85.0*CTRL_DEG2RAD;
        joint3_1=105.0*CTRL_DEG2RAD;
        joint4_0= 90.0*CTRL_DEG2RAD;
        joint4_1= 40.0*CTRL_DEG2RAD;
        elb_m=(joint4_1-joint4_0)/(joint3_1-joint3_0);
        elb_n=joint4_0-elb_m*joint3_0;
    }

    /****************************************************************/
    void ControllerNLP::computeGuard()
    {
        double guardRatio=0.1;
        qGuard.resize(chain_dof);
        qGuardMinExt.resize(chain_dof);
        qGuardMinInt.resize(chain_dof);
        qGuardMinCOG.resize(chain_dof);
        qGuardMaxExt.resize(chain_dof);
        qGuardMaxInt.resize(chain_dof);
        qGuardMaxCOG.resize(chain_dof);

        for (size_t i=0; i < chain_dof; i++)
        {
            qGuard[i]=0.25*guardRatio*(q_lim(i,1)-q_lim(i,0));

            qGuardMinExt[i]=q_lim(i,0)+qGuard[i];
            qGuardMinInt[i]=qGuardMinExt[i]+qGuard[i];
            qGuardMinCOG[i]=0.5*(qGuardMinExt[i]+qGuardMinInt[i]);

            qGuardMaxExt[i]=q_lim(i,1)-qGuard[i];
            qGuardMaxInt[i]=qGuardMaxExt[i]-qGuard[i];
            qGuardMaxCOG[i]=0.5*(qGuardMaxExt[i]+qGuardMaxInt[i]);
        }
    }

    /****************************************************************/
    void ControllerNLP::computeBounds()
    {
        for (size_t i=0; i < chain_dof; i++)
        {
            double qi=q0[i];
            if ((qi>=qGuardMinInt[i]) && (qi<=qGuardMaxInt[i]))
                bounds(i,0)=bounds(i,1)=1.0;
            else if (qi<qGuardMinInt[i])
            {
                bounds(i,0)=(qi<=qGuardMinExt[i]?0.0:
                             0.5*(1.0+tanh(+10.0*(qi-qGuardMinCOG[i])/qGuard[i])));
                bounds(i,1)=1.0;
            }
            else
            {
                bounds(i,0)=1.0;
                bounds(i,1)=(qi>=qGuardMaxExt[i]?0.0:
                             0.5*(1.0+tanh(-10.0*(qi-qGuardMaxCOG[i])/qGuard[i])));
            }
        }

        for (size_t i=0; i < chain_dof; i++)
        {
            bounds(i,0)*=v_lim(i,0);
            bounds(i,1)*=v_lim(i,1);
        }
    }

    /****************************************************************/
    void ControllerNLP::computeDimensions()
    {
        // reaching in position
        constr_num=3; nnz_jacobian=chain_dof*3;

        // reaching in orientation
        constr_num += 3; nnz_jacobian += chain_dof*3;

        if (hitting_constraints)
        {
            // shoulder's cables length
            constr_num += 3;
            nnz_jacobian += 2 + 3 + 2;

            // avoid hitting torso
            constr_num += 1;
            nnz_jacobian += 2;

            // avoid hitting forearm
            constr_num += 2;
            nnz_jacobian += 2 + 2;
        }
    }

    /****************************************************************/
    Matrix ControllerNLP::v2m(const Vector &x)
    {
        yAssert(x.length()>=6)
        Vector ang=x.subVector(3,5);
        double ang_mag=norm(ang);
        if (ang_mag>0.0)
            ang/=ang_mag;
        ang.push_back(ang_mag);
        Matrix H=axis2dcm(ang);
        H(0,3)=x[0];
        H(1,3)=x[1];
        H(2,3)=x[2];
        return H;
    }

    /****************************************************************/
    Matrix ControllerNLP::skew(const Vector &w)
    {
        yAssert(w.length()>=3)
        Matrix S(3,3);
        S(0,0)=S(1,1)=S(2,2)=0.0;
        S(1,0)= w[2]; S(0,1)=-S(1,0);
        S(2,0)=-w[1]; S(0,2)=-S(2,0);
        S(2,1)= w[0]; S(1,2)=-S(2,1);
        return S;
    }

//public:
    /****************************************************************/
    ControllerNLP::ControllerNLP(iKinChain &chain_, std::vector<ControlPoint> &additional_control_points_,
                                 bool hittingConstraints_, bool orientationControl_, bool additionalControlPoints_,
                                 double dT_, double restPosWeight_) :
            chain(chain_), additional_control_points(additional_control_points_), hitting_constraints(hittingConstraints_),
            ori_control(orientationControl_), additional_control_points_flag(additionalControlPoints_),
            dt(dT_), nnz_jacobian(0), constr_num(0), extra_ctrl_points_nr(0), additional_control_points_tol(0.0001),
            shou_m(0), shou_n(0), elb_m(0), elb_n(0), w1(1), w2(restPosWeight_), w3(1), w4(0)
    {
        chain_dof = static_cast<int>(chain.getDOF());
        xr.resize(6,0.0);
        pr.resize(3,0.0);
        v0.resize(chain_dof, 0.0); v=v0;
        q_lim.resize(chain_dof, 2);
        v_lim.resize(chain_dof, 2);
        for (size_t r=0; r < chain_dof; r++)
        {
            q_lim(r,0)=chain(r).getMin();
            q_lim(r,1)=chain(r).getMax();

            v_lim(r,1)=std::numeric_limits<double>::max();
            v_lim(r,0)=-v_lim(r,1);
        }
        bounds=v_lim;
        computeSelfAvoidanceConstraints();
        computeGuard();
        computeDimensions();
        rest_jnt_pos = {0, 0, 0, -25*CTRL_DEG2RAD, 20*CTRL_DEG2RAD, 0, 50*CTRL_DEG2RAD, 0, -20*CTRL_DEG2RAD, 0};
        rest_weights = {1,1,1,0,0,0,0,0,0,0};
        rest_err.resize(chain_dof, 0.0);
    }

    ControllerNLP::~ControllerNLP() = default;

    /****************************************************************/
    void ControllerNLP::init(const Vector &_xr, const Vector &_v0, const Matrix &_v_lim)
    {
        yAssert(xr.length() == _xr.length())
        yAssert(v0.length() == _v0.length())
        yAssert((v_lim.rows() == _v_lim.rows()) && (v_lim.cols() == _v_lim.cols()))
        for (int r=0; r < _v_lim.rows(); r++)
            yAssert(_v_lim(r, 0) <= _v_lim(r, 1));

        v_lim= CTRL_DEG2RAD * _v_lim;
        v0= CTRL_DEG2RAD * _v0;
        q0=chain.getAng();
        H0=chain.getH();
        p0=H0.getCol(3).subVector(0,2);
        xr=_xr;
        pr=_xr.subVector(0, 2);

        Matrix R = v2m(_xr).submatrix(0,2,0,2)*H0.submatrix(0,2,0,2).transposed();
        v_des.resize(6,0);
        v_des.setSubvector(0, (pr-p0) / dt);
        v_des.setSubvector(3, dcm2rpy(R) / dt);

        //printf("[ControllerNLP::init()]: getH() - end-effector: \n %s\n",H0.toString(3,3).c_str());
        //printf("[ControllerNLP::init()]: p0 - end-effector: (%s)\n",p0.toString(3,3).c_str());
          
        J0=chain.GeoJacobian();
        if (additional_control_points_flag)
        {
//            additional_control_points_tol = 0.0001; //norm2 is used in the g function, so this is the Eucl. distance error squared - actual distance is sqrt(additional_control_points_tol);
            //e.g., 0.0001 corresponds to 0.01 m error in actual Euclidean distance 
            //N.B. for the end-effector, tolerance is 0
            if (additional_control_points.empty())
            {
                yWarning("[ControllerNLP::init()]: additional_control_points_flag is on but additional_control_points.size is 0.");
                additional_control_points_flag = false;
                extra_ctrl_points_nr = 0;                
            }
            else
            { 
                if (additional_control_points.size() == 1)
                {
                    extra_ctrl_points_nr = 1;
                }
                else  //additional_control_points.size() > 1
                {
                    extra_ctrl_points_nr = 1;
                    yWarning("[ControllerNLP::init()]: currently only one additional control point - Elbow - is supported; requested %lu control points.",additional_control_points.size());
                }
                for (auto & additional_control_point : additional_control_points)
                {
                    if(additional_control_point.type == "Elbow")
                    {
                        /*Matrix H4=chain.getH(chain_dof-5-1);
                        Vector p4 = H4.getCol(3).subVector(0,2);
                        printf("[ControllerNLP::init()]: getH(%d): \n %s \n",chain_dof-5-1,H4.toString(3,3).c_str());
                        printf("[ControllerNLP::init()]: p4: (%s)\n",p4.toString(3,3).c_str());
                        */
                        Matrix H5=chain.getH(chain_dof - 4 - 1);
                        additional_control_point.p0 = H5.getCol(3).subVector(0,2);
                        //printf("[ControllerNLP::init()]: getH(%d) - elbow: \n %s \n",chain_dof-4-1,H5.toString(3,3).c_str());
                        //yInfo("[ControllerNLP::init()]: p0 - current pos - elbow: (%s)\n",(*it).p0.toString(3,3).c_str());
                        /*
                        Matrix H6=chain.getH(chain_dof-3-1);
                        Vector p6 = H6.getCol(3).subVector(0,2);
                        printf("[ControllerNLP::init()]: getH(%d): \n %s \n",chain_dof-3-1,H6.toString(3,3).c_str());
                        printf("[ControllerNLP::init()]: p6: (%s)\n",p6.toString(3,3).c_str());
                        Matrix H7=chain.getH(chain_dof-2-1);
                        Vector p7 = H7.getCol(3).subVector(0,2);
                        printf("[ControllerNLP::init()]: getH(%d): \n %s \n",chain_dof-2-1,H7.toString(3,3).c_str());
                        printf("[ControllerNLP::init()]: p7: (%s)\n",p7.toString(3,3).c_str());
                        Matrix H8=chain.getH(chain_dof-1-1);
                        Vector p8 = H8.getCol(3).subVector(0,2);
                        printf("[ControllerNLP::init()]: getH(%d): \n %s \n",chain_dof-1-1,H8.toString(3,3).c_str());
                        printf("[ControllerNLP::init()]: p8: (%s)\n",p8.toString(3,3).c_str());
                        Matrix H9=chain.getH(chain_dof-1);
                        Vector p9 = H9.getCol(3).subVector(0,2);
                        printf("[ControllerNLP::init()]: getH(%d): \n %s \n",chain_dof-1,H9.toString(3,3).c_str());
                        printf("[ControllerNLP::init()]: p9: (%s)\n",p9.toString(3,3).c_str());
                        */
                        
                        //yInfo("[ControllerNLP::init()]: current elbow position (p0) in ipopt chain: (%s)",(*it).p0.toString(3,3).c_str());
                        Matrix J = chain.GeoJacobian(chain_dof - 4 - 1);
                        additional_control_point.J0_xyz = J.submatrix(0, 2, 0, chain_dof - 4 - 1);
                        //yInfo("[ControllerNLP::init()]: elbow J0_xyz: \n %s \n",(*it).J0_xyz.toString().c_str());
                    }
                    else
                        yWarning("[ControllerNLP::get_nlp_info]: other control points type than Elbow are not supported (this was %s).",additional_control_point.type.c_str());
                }
            }
        }
        else
            extra_ctrl_points_nr = 0;
        computeBounds();
    }


    /****************************************************************/
    bool ControllerNLP::get_nlp_info(Ipopt::Index &n, Ipopt::Index &m, Ipopt::Index &nnz_jac_g,
                      Ipopt::Index &nnz_h_lag, IndexStyleEnum &index_style)
    {
        n = chain_dof + 6;
        m = constr_num;
        nnz_jac_g = nnz_jacobian+6;

        if(additional_control_points_flag)
        {
            for (const auto & additional_control_point : additional_control_points)
            {
                if(additional_control_point.type == "Elbow")
                {
                    m+=1; nnz_jac_g += (chain_dof - 4); //taking out 2 wrist and 2 elbow joints
                }
                else
                    yWarning("[ControllerNLP::get_nlp_info]: other control points type than Elbow are not supported (this was %s).",
                             additional_control_point.type.c_str());
            }
        }

        nnz_h_lag= chain_dof;
        index_style=TNLP::C_STYLE;
        return true;
    }

    /****************************************************************/
    bool ControllerNLP::get_bounds_info(Ipopt::Index n, Ipopt::Number *x_l, Ipopt::Number *x_u,
                         Ipopt::Index m, Ipopt::Number *g_l, Ipopt::Number *g_u) {
        for (Ipopt::Index i = 0; i < chain_dof; i++) {
            if (bounds(i,0) < bounds(i,1)) {
                x_l[i] = bounds(i, 0);
                x_u[i] = bounds(i, 1);
            } else {
                if (bounds(i,1) < 0 && bounds(i,0) > 0)
                    x_l[i] = x_u[i] = 0;
                else if (abs(bounds(i,0)) < abs(bounds(i,1)) )
                    x_l[i] = x_u[i] = bounds(i,0);
                else
                    x_l[i] = x_u[i] = bounds(i,1);
            }
        }
        for (int i = 0; i < 3; ++i) {
            x_l[chain_dof+i] = 0.0;
            x_u[chain_dof+i] = 0.0; // 1.5
        }

        for (int i = 3; i < 6; ++i) {
            x_l[chain_dof+i] = 0.0; // -std::numeric_limits<double>::max();
            x_u[chain_dof+i] = 0.0; // std::numeric_limits<double>::max(); // 10; // 0.04;
        }

        // reaching in position - upper and lower bounds on the error (Euclidean square norm)
//        g_l[0]= -std::numeric_limits<double>::max();
//        g_u[0]= 1e-6;
        for (int i = 0; i < 3; ++i) {
            g_l[i] = -1e-4;
            g_u[i] = 1e-4;
        }

        // reaching orientation
        for (int i = 3; i < 6; ++i) {
            g_l[i] = -0.4; //-1e-2
            g_u[i] = 0.4; //-1e-2 // 10; // 0.04;
        }


        if(additional_control_points_flag)
        {
            for(Ipopt::Index j=0; j<extra_ctrl_points_nr; j++){
                g_l[6+j]=0.0;
                g_u[6+j]= additional_control_points_tol;
            }
        }

        if (hitting_constraints)
        {
            // shoulder's cables length
            g_l[6+extra_ctrl_points_nr]=-347.00*CTRL_DEG2RAD;
            g_u[6+extra_ctrl_points_nr]=std::numeric_limits<double>::max();
            g_l[7+extra_ctrl_points_nr]=-366.57*CTRL_DEG2RAD;
            g_u[7+extra_ctrl_points_nr]=112.42*CTRL_DEG2RAD;
            g_l[8+extra_ctrl_points_nr]=-66.60*CTRL_DEG2RAD;
            g_u[8+extra_ctrl_points_nr]=213.30*CTRL_DEG2RAD;

            // avoid hitting torso
            g_l[9+extra_ctrl_points_nr]=shou_n;
            g_u[9+extra_ctrl_points_nr]=std::numeric_limits<double>::max();

            // avoid hitting forearm
            g_l[10+extra_ctrl_points_nr]=-std::numeric_limits<double>::max();
            g_u[10+extra_ctrl_points_nr]=elb_n;
            g_l[11+extra_ctrl_points_nr]=-elb_n;
            g_u[11+extra_ctrl_points_nr]=std::numeric_limits<double>::max();
        }

        return true;
    }

    /****************************************************************/
    bool ControllerNLP::get_starting_point(Ipopt::Index n, bool init_x, Ipopt::Number *x,
                            bool init_z, Ipopt::Number *z_L, Ipopt::Number *z_U,
                            Ipopt::Index m, bool init_lambda, Ipopt::Number *lambda)
    {
        for (Ipopt::Index i = 0; i < chain_dof; i++)
            x[i] = std::min(std::max(bounds(i, 0), v0[i]), bounds(i, 1));
        for (Ipopt::Index i = 0; i < 6; i++) {
            x[chain_dof+i] = 0.0;
        }
        return true;
    }

    /************************************************************************/
    void ControllerNLP::computeQuantities(const Ipopt::Number *x, const bool new_x)
    {
        if (new_x)
        {
            q1 = q0;
            for (size_t i = 0; i < chain_dof; i++) {
                v[i] = x[i];
                q1[i] += dt * v[i];
            }
            rest_err = rest_weights * (rest_jnt_pos-q1);

            v_x = J0 *v;

            if (additional_control_points_flag)
            {
                for (const auto & additional_control_point : additional_control_points)
                {
                    if(additional_control_point.type == "Elbow")
                    {
                        //yInfo("[ControllerNLP::computeQuantities]: will compute solved elbow position as: (%s) + %f * \n %s * \n (%s)",(*it).p0.toString(3,3).c_str(),dt,(*it).J0_xyz.toString(3,3).c_str(), v.subVector(0,v0.length()-4-1).toString(3,3).c_str());
                        Vector pe_elbow = additional_control_point.p0 + dt* (additional_control_point.J0_xyz * v.subVector(0,v0.length()-4-1));
                        err_xyz_elbow = additional_control_point.x_desired - pe_elbow;
                        //yInfo("[ControllerNLP::computeQuantities]: Compute error in solved elbow position: (%s) = (desired - computed) =  (%s) - (%s)",err_xyz_elbow.toString(3,3).c_str(),(*it).x_desired.toString(3,3).c_str(),pe_elbow.toString(3,3).c_str()); 
                    }
                    else
                        yWarning("[ControllerNLP::computeQuantities]: other control points type than Elbow are not supported (this was %s).",additional_control_point.type.c_str());
                }
            }
        }
    }

    /****************************************************************/
    bool ControllerNLP::eval_f(Ipopt::Index n, const Ipopt::Number *x, bool new_x,
                Ipopt::Number &obj_value)
    {
        computeQuantities(x,new_x);
        obj_value = w1 * (dot(v,v)-2*dot(v,v0)) +  // changed norm(v1-v0)^2 to dot(v1,v1) - 2dot(v1,v0)
                w3 * (abs(x[chain_dof]) + abs(x[chain_dof + 1]) + abs(x[chain_dof + 2])) +
                (ori_control ? w4 * (abs(x[chain_dof + 3]) + abs(x[chain_dof + 4]) + abs(x[chain_dof + 5])) : 0.0);
        for (Ipopt::Index i=0; i < chain_dof; i++) {
            obj_value+= w2 * rest_weights[i]* rest_weights[i]* dt*v[i] * (2*(q0[i] - rest_jnt_pos[i]) + dt * v[i]);
        }
        return true;
    }

    /****************************************************************/
    bool ControllerNLP::eval_grad_f(Ipopt::Index n, const Ipopt::Number* x, bool new_x,
                     Ipopt::Number *grad_f)
    {
        computeQuantities(x,new_x);
        for (Ipopt::Index i=0; i < chain_dof; i++) {
            grad_f[i] =  2.0 * w1 * (v[i] - v0[i]) - 2.0 * dt * w2 * rest_weights[i] * rest_err[i];
        }
        grad_f[chain_dof] = w3; grad_f[chain_dof+1] = w3; grad_f[chain_dof+2] = w3;
        grad_f[chain_dof+3] = (ori_control ? w4 : 0.0); grad_f[chain_dof+4] = (ori_control ? w4 : 0.0);
        grad_f[chain_dof+5] = (ori_control ? w4 : 0.0);
        return true;
    }

    bool ControllerNLP::eval_h(Ipopt::Index n, const Ipopt::Number *x, bool new_x, Ipopt::Number obj_factor, Ipopt::Index m, const Ipopt::Number *lambda,
                               bool new_lambda, Ipopt::Index nele_hess, Ipopt::Index *iRow, Ipopt::Index *jCol, Ipopt::Number *values)
    {
        if (values==nullptr)
        {
            for (Ipopt::Index row=0; row<chain_dof; row++)
            {
                iRow[row]=row; jCol[row]=row;
            }
        }
        else
        {
            for (int i = 0; i < chain_dof; ++i)
                values[i] = obj_factor*(2.0 * w1 + 2.0 * dt * dt * w2 * rest_weights[i] * rest_weights[i]);
        }

        return true;
    }

    /****************************************************************/
    bool ControllerNLP::eval_g(Ipopt::Index n, const Ipopt::Number *x, bool new_x,
                Ipopt::Index m, Ipopt::Number *g)
    {
        computeQuantities(x,new_x);

        // reaching - end-effector
        for (int i = 0; i < 6; ++i) {
            g[i] = v_des(i) - v_x(i) + x[chain_dof+i]; // norm2(err_xyz)-x[chain_dof]; // ang_mag*ang_mag-x[chain_dof+1];
        }

        if (additional_control_points_flag)
        {
            //yInfo("ControllerNLP::eval_g(): additional_control_points_flag on, extra_ctrl_points_nr: %d",extra_ctrl_points_nr);
            g[6] = norm2(err_xyz_elbow); //this is hard-coded for the elbow as the only extra control point
            //yInfo("\t g[0]: %.5f, g[1]: %.5f",g[0],g[1]);
        }
        if (hitting_constraints)
        {
            // shoulder's cables length
            g[6 + extra_ctrl_points_nr] = 1.71 * (q1[3 + 0] - q1[3 + 1]);
            g[7 + extra_ctrl_points_nr] = 1.71 * (q1[3 + 0] - q1[3 + 1] - q1[3 + 2]);
            g[8 + extra_ctrl_points_nr] = q1[3 + 1] + q1[3 + 2];

            // avoid hitting torso
            g[9 + extra_ctrl_points_nr] = q1[3 + 1] - shou_m * q1[3 + 2];

            // avoid hitting forearm
            g[10 + extra_ctrl_points_nr] = -elb_m * q1[3 + 3 + 0] + q1[3 + 3 + 1];
            g[11 + extra_ctrl_points_nr] = elb_m * q1[3 + 3 + 0] + q1[3 + 3 + 1];
        }
        return true;
    }

    /****************************************************************/
    bool ControllerNLP::eval_jac_g(Ipopt::Index n, const Ipopt::Number *x, bool new_x,
                    Ipopt::Index m, Ipopt::Index nele_jac, Ipopt::Index *iRow,
                    Ipopt::Index *jCol, Ipopt::Number *values)
    {
        if (values==nullptr)
        {
            Ipopt::Index idx=0;

            // reaching in position and orientation - end-effector
            for (int j = 0; j < 6; ++j) {
                for (Ipopt::Index i = 0; i < chain_dof; i++) {
                    iRow[idx] = j; jCol[idx] = i; idx++;
                }
                iRow[idx] = j; jCol[idx] = chain_dof+j; idx++;
            }

            if (additional_control_points_flag)
            {
                for (const auto & additional_control_point : additional_control_points)
                {
                    if(additional_control_point.type == "Elbow")
                    {
                         // reaching in position - elbow control point
                        for (Ipopt::Index j=0; j<(chain_dof - 4); j++)
                        {
                            iRow[idx]=6; jCol[idx]=j;
                            idx++;
                        }
                    }
                    else
                        yWarning("[ControllerNLP::eval_jac_g]: other control points type than Elbow are not supported (this was %s).",additional_control_point.type.c_str());
                }                
            }
            
            if (hitting_constraints)
            {
                // shoulder's cables length
                iRow[idx]=6+extra_ctrl_points_nr; jCol[idx]=3+0; idx++;
                iRow[idx]=6+extra_ctrl_points_nr; jCol[idx]=3+1; idx++;

                iRow[idx]=7+extra_ctrl_points_nr; jCol[idx]=3+0; idx++;
                iRow[idx]=7+extra_ctrl_points_nr; jCol[idx]=3+1; idx++;
                iRow[idx]=7+extra_ctrl_points_nr; jCol[idx]=3+2; idx++;

                iRow[idx]=8+extra_ctrl_points_nr; jCol[idx]=3+1; idx++;
                iRow[idx]=8+extra_ctrl_points_nr; jCol[idx]=3+2; idx++;

                // avoid hitting torso
                iRow[idx]=9+extra_ctrl_points_nr; jCol[idx]=3+1; idx++;
                iRow[idx]=9+extra_ctrl_points_nr; jCol[idx]=3+2; idx++;

                // avoid hitting forearm
                iRow[idx]=10+extra_ctrl_points_nr; jCol[idx]=3+3+0; idx++;
                iRow[idx]=10+extra_ctrl_points_nr; jCol[idx]=3+3+1; idx++;

                iRow[idx]=11+extra_ctrl_points_nr; jCol[idx]=3+3+0; idx++;
                iRow[idx]=11+extra_ctrl_points_nr; jCol[idx]=3+3+1;
            }
        }
        else
        {
            computeQuantities(x,new_x);

            Ipopt::Index idx=0;

            // reaching in position
            for (int j = 0; j < 6; ++j) {
                for (Ipopt::Index i = 0; i < chain_dof; i++)
                    values[idx++] = -J0(j, i);
                values[idx++] = 1;
            }

            if (additional_control_points_flag)
            {
                for (const auto & additional_control_point : additional_control_points)
                {
                    if(additional_control_point.type == "Elbow")
                    {
                         // reaching in position - elbow control point
                        for (Ipopt::Index j=0; j<(chain_dof - 4); j++)
                        {
                            values[idx]=-2.0*dt*dot(err_xyz_elbow,additional_control_point.J0_xyz.getCol(j));
                            idx++;
                        }
                    }
                    else
                        yWarning("[ControllerNLP::eval_jac_g]: other control points type than Elbow are not supported (this was %s).",
                                 additional_control_point.type.c_str());
                }                
            }
            
            
            if (hitting_constraints)
            {
                // shoulder's cables length
                values[idx++] = 1.71 * dt;
                values[idx++] = -1.71 * dt;

                values[idx++] = 1.71 * dt;
                values[idx++] = -1.71 * dt;
                values[idx++] = -1.71 * dt;

                values[idx++] = dt;
                values[idx++] = dt;

                // avoid hitting torso
                values[idx++] = dt;
                values[idx++] = -shou_m * dt;

                // avoid hitting forearm
                values[idx++] = -elb_m * dt;
                values[idx++] = dt;

                values[idx++] = elb_m * dt;
                values[idx] = dt;
            }
        }
        return true;
    }

    /****************************************************************/
    void ControllerNLP::finalize_solution(Ipopt::SolverReturn status, Ipopt::Index n,
                           const Ipopt::Number *x, const Ipopt::Number *z_L,
                           const Ipopt::Number *z_U, Ipopt::Index m,
                           const Ipopt::Number *g, const Ipopt::Number *lambda,
                           Ipopt::Number obj_value, const Ipopt::IpoptData *ip_data,
                           Ipopt::IpoptCalculatedQuantities *ip_cq)
    {
        for (Ipopt::Index i=0; i < chain_dof; i++)
            v[i]=x[i];
    }
