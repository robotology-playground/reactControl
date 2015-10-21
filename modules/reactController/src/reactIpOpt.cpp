/* 
 * Copyright: (C) 2015 iCub Facility - Istituto Italiano di Tecnologia
 * Author: Alessandro Roncone <alessandro.roncone@iit.it>
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

#include <limits>
#include <sstream>

#include <IpTNLP.hpp>
#include <IpIpoptApplication.hpp>

#include "assert.h"

#include "reactIpOpt.h"

#define CAST_IPOPTAPP(x)             (static_cast<IpoptApplication*>(x))

using namespace std;
using namespace yarp::os;
using namespace yarp::sig;
using namespace yarp::math;
using namespace iCub::ctrl;
using namespace iCub::iKin;
using namespace Ipopt;

/************************************************************************/
class react_NLP : public TNLP
{
private:
    // Copy constructor: not implemented.
    react_NLP(const react_NLP&);
    // Assignment operator: not implemented.
    react_NLP &operator=(const react_NLP&);

protected:
    // The name of the class instance (fixed for now to react_NLP)
    string name;
    // The verbosity level (fixed for now to 0)
    int verbosity;
    // The chain that will undergo the task
    iKinChain &chain;
    // The inequality constraints (if available)
    iKinLinIneqConstr &LIC;
    // The dimensionality of the task (for now it should be 7)
    unsigned int dim;

    // The desired position to attain
    yarp::sig::Vector &xd;
    // The current position
    yarp::sig::Vector x0;

    // The delta T with which ipopt needs to solve the task
    double &dT;

    // The desired final joint velocities
    yarp::sig::Vector q_dot_d;
    // The initial joint velocities
    yarp::sig::Vector q_dot_0;
    // The current joint velocities
    yarp::sig::Vector q_dot;

    // The current joint configuration
    yarp::sig::Vector q_t;

    // The cost function
    yarp::sig::Vector cost_func;
    // The gradient of the cost function
    yarp::sig::Vector grad_cost_func;
    // The jacobian associated with the const function
    yarp::sig::Matrix J_cst;

    yarp::sig::Vector linC;

    double __obj_scaling;
    double __x_scaling;
    double __g_scaling;
    double lowerBoundInf;
    double upperBoundInf;

    bool   firstGo;

    /************************************************************************/
    virtual void computeQuantities(const Number *x)
    {
        printMessage(9,"[computeQuantities] START dim: %i \n", dim);
        yarp::sig::Vector new_q_dot(dim,0.0);

        for (Index i=0; i<(int)dim; i++)
        {
            new_q_dot[i]=x[i];
        }
        if (!(q_dot==new_q_dot) || firstGo)
        {
            firstGo=false;
            q_dot=new_q_dot;
            yarp::sig::Matrix J1=chain.GeoJacobian();
            submatrix(J1,J_cst,0,2,0,dim-1);

            q_t = chain.getAng();
            cost_func=xd -(x0+dT*J_cst*q_dot);
            grad_cost_func=2*cost_func*(-dT*J_cst);

            // if (LIC.isActive())
            //     linC=LIC.getC()*q_t;
        }
        printMessage(9,"[computeQuantities] OK x: %s\n",IPOPT_Number_toString(x,CTRL_RAD2DEG).c_str());
    }

    /************************************************************************/
    string IPOPT_Number_toString(const Number* x, const double multiplier=1)
    {
        std::ostringstream ss;
        for (Index i=0; i<dim; i++)
        {
            ss << x[i]*multiplier << " ";
        }
        return ss.str();
    }

    /************************************************************************/
    int printMessage(const int l, const char *f, ...) const
    {
        if (verbosity>=l)
        {
            fprintf(stdout,"[%s] ",name.c_str());

            va_list ap;
            va_start(ap,f);
            int ret=vfprintf(stdout,f,ap);
            va_end(ap);
            return ret;
        }
        else
            return -1;
    }


public:
    /************************************************************************/
    react_NLP(iKinChain &c, yarp::sig::Vector &_xd, yarp::sig::Vector &_q_dot_0,
             double &_dT, iKinLinIneqConstr &_LIC, int _verbosity) : chain(c),
             q_dot_0(_q_dot_0), dT(_dT), xd(_xd), LIC(_LIC), verbosity(_verbosity)
    {
        name="react_NLP";

        // A time should always be positive
        if (dT<0.0)
        {
            dT=0.05;
        }

        x0.resize(3,0.0);
        yarp::sig::Matrix H=chain.getH();
        x0=H.subcol(0,3,3);

        dim=chain.getDOF();
        q_dot.resize(dim,0.0);
        q_dot_d.resize(dim,0.0);

        cost_func.resize(3,0.0);
        J_cst.resize(3,dim); J_cst.zero();

        firstGo=true;

        __obj_scaling=1.0;
        __x_scaling  =1.0;
        __g_scaling  =1.0;

        lowerBoundInf=-std::numeric_limits<double>::max();
        upperBoundInf=std::numeric_limits<double>::max();
    }

    /************************************************************************/
    yarp::sig::Vector get_q_dot_d() { return q_dot_d; }

    /************************************************************************/
    void set_scaling(double _obj_scaling, double _x_scaling, double _g_scaling)
    {
        __obj_scaling=_obj_scaling;
        __x_scaling  =_x_scaling;
        __g_scaling  =_g_scaling;
        printMessage(7,"[set_scaling] OK\n");
    }

    /************************************************************************/
    void set_bound_inf(double lower, double upper)
    {
        lowerBoundInf=lower;
        upperBoundInf=upper;
        printMessage(7,"[set_bound_inf] OK\n");
    }

    /************************************************************************/
    bool get_nlp_info(Index& n, Index& m, Index& nnz_jac_g, Index& nnz_h_lag,
                      IndexStyleEnum& index_style)
    {
        n=dim;
        m=dim+0;
        nnz_jac_g=dim; // the jacobian has dim non zero entries (the diagonal)

        // if (LIC.isActive())
        // {
        //     int lenLower=LIC.getlB().length();
        //     int lenUpper=LIC.getuB().length();

        //     if (lenLower && (lenLower==lenUpper) && (LIC.getC().cols()==dim))
        //     {
        //         m+=lenLower;
        //         nnz_jac_g+=lenLower*dim;
        //     }
        //     else
        //         LIC.setActive(false);
        // }
        
        // nnz_h_lag=(dim*(dim+1))>>1;
        nnz_h_lag=0;
        index_style=TNLP::C_STYLE;
        printMessage(7,"[get_nlp_info]\tn: %i m: %i nnz_jac_g: %i\n",n,m,nnz_jac_g);
        
        return true;
    }
    
    /************************************************************************/
    bool get_starting_point(Index n, bool init_x, Number* x, bool init_z,
                            Number* z_L, Number* z_U, Index m, bool init_lambda,
                            Number* lambda)
    {
        assert(init_x == true);
        assert(init_z == false);
        assert(init_lambda == false);

        for (Index i=0; i<n; i++)
            x[i]=q_dot_0[i];

        printMessage(7,"[get_starting_pnt]  OK n: %i x_0: %s\n",n,IPOPT_Number_toString(x).c_str());

        return true;
    }

    /************************************************************************/
    bool get_bounds_info(Index n, Number* x_l, Number* x_u, Index m, Number* g_l,
                         Number* g_u)
    {
        for (Index i=0; i<n; i++)
        {
            // x_l[i]=chain(i).getMin();
            // x_u[i]=chain(i).getMax();
            // Let's put these limits to the velocities for the time being
            x_l[i]=-50.0*CTRL_DEG2RAD;
            x_u[i]=+50.0*CTRL_DEG2RAD;
        }
        
        for (Index i=0; i<m; i++)
        {
            if (i<dim)
            {
                g_l[i]=chain(i).getMin();
                g_u[i]=chain(i).getMax();
            }
            // if (i>=dim)
            // {
            //     g_l[i]=LIC.getlB()[i-dim];
            //     g_u[i]=LIC.getuB()[i-dim];
            // }
        }
        
        printMessage(7,"[get_bounds_info]   n: %i m: %i\n",n,m);
        printMessage(9,"[get_bounds_info]   x_l: %s\n", IPOPT_Number_toString(x_l).c_str());
        printMessage(9,"[get_bounds_info]   x_u: %s\n", IPOPT_Number_toString(x_u).c_str());
        printMessage(9,"[get_bounds_info]   g_l: %s\n", IPOPT_Number_toString(g_l).c_str());
        printMessage(9,"[get_bounds_info]   g_u: %s\n", IPOPT_Number_toString(g_u).c_str());
        return true;
    }
    
    /************************************************************************/
    bool eval_f(Index n, const Number* x, bool new_x, Number& obj_value)
    {
        computeQuantities(x);

        obj_value=norm2(cost_func);
        printMessage(7,"[eval_f] OK\t\tcost_func: %s\tobj_value %g\n",cost_func.toString().c_str(),obj_value);

        return true;
    }
    
    /************************************************************************/
    bool eval_grad_f(Index n, const Number* x, bool new_x, Number* grad_f)
    {
        computeQuantities(x);

        for (Index i=0; i<n; i++)
            grad_f[i]=grad_cost_func[i];
            
        printMessage(7,"[eval_grad_f] OK\n");
        return true;
    }
    
    /************************************************************************/
    bool eval_g(Index n, const Number* x, bool new_x, Index m, Number* g)
    {
        printMessage(9,"[eval_g]\t\tq(t): %s\n",(q_t*CTRL_RAD2DEG).toString(3,3).c_str());
        computeQuantities(x);

        for (Index i=0; i<m; i++)
        {
            if (i<dim)
            {
                g[i]=q_t(i) + dT * q_dot(i);
            }
            if (i>=dim)
            {
                g[i]=linC[i-dim];
            }
        }
        printMessage(7,"[eval_g] OK\t\tq(t+1): %s\n",IPOPT_Number_toString(g,CTRL_RAD2DEG).c_str());

        return true;
    }
    
    /************************************************************************/
    bool eval_jac_g(Index n, const Number* x, bool new_x, Index m, Index nele_jac,
                    Index* iRow, Index *jCol, Number* values)
    {
        // printMessage(9,"[eval_jac_g] START\tx: %i\n",IPOPT_Number_toString(x,CTRL_RAD2DEG).c_str());
        // computeQuantities(x);

        if (m>=n) // if there are at least the joint bounds as constraint
        {
            if (values==NULL)
            {
                Index idx=0;
                
                // Let's populate the diagonal matrix with dT
                for (Index i=0; i<m; i++)
                {
                    iRow[idx]=i;
                    jCol[idx]=i;
                    idx++;
                }
            }
            else
            {
                computeQuantities(x);

                Index idx=0;
                
                // Let's populate the diagonal matrix with dT
                for (Index i=0; i<m; i++)
                {
                    values[idx]=dT;
                    idx++;
                }
            }
        }

        printMessage(7,"[eval_jac_g] OK\n");
        return true;
    }
    
    /************************************************************************/
    // bool eval_h(Index n, const Number* x, bool new_x, Number obj_factor,
    //             Index m, const Number* lambda, bool new_lambda,
    //             Index nele_hess, Index* iRow, Index* jCol, Number* values)
    // {
    //     // Empty for now
    //     computeQuantities(x);

    //     return true;
    // }

    /************************************************************************/
    bool get_scaling_parameters(Number& obj_scaling, bool& use_x_scaling, Index n,
                                Number* x_scaling, bool& use_g_scaling, Index m,
                                Number* g_scaling)
    {
        printMessage(9,"[get_scaling_parameters] START");
        obj_scaling=__obj_scaling;

        for (Index i=0; i<n; i++)
            x_scaling[i]=__x_scaling;

        for (Index j=0; j<m; j++)
            g_scaling[j]=__g_scaling;

        use_x_scaling=use_g_scaling=true;

        printMessage(7,"[get_scaling_parameters] END\n");

        return true;
    }

    /************************************************************************/
    void finalize_solution(SolverReturn status, Index n, const Number* x,
                           const Number* z_L, const Number* z_U, Index m,
                           const Number* g, const Number* lambda, Number obj_value,
                           const IpoptData* ip_data, IpoptCalculatedQuantities* ip_cq)
    {
        // Let's write the solution to the console
        printMessage(4,"[finalize_solution] Solution of the primal variables, x: %s\n",
                                        IPOPT_Number_toString(x,CTRL_RAD2DEG).c_str());
        printMessage(4,"[finalize_solution] Solution of the bound multipliers: z_L and z_U\n");
        printMessage(4,"[finalize_solution] z_L: %s\n",
                        IPOPT_Number_toString(z_L,CTRL_RAD2DEG).c_str());
        printMessage(4,"[finalize_solution] z_U: %s\n",
                        IPOPT_Number_toString(z_U,CTRL_RAD2DEG).c_str());
        printMessage(4,"[finalize_solution] q(t+1): %s\n",
                        IPOPT_Number_toString(g,CTRL_RAD2DEG).c_str());
        printMessage(4,"[finalize_solution] Objective value f(x*) = %e\n", obj_value);
        for (Index i=0; i<n; i++)
            q_dot_d[i]=x[i];
    }

    /************************************************************************/
    virtual ~react_NLP() { }
};


/************************************************************************/
reactIpOpt::reactIpOpt(iKinChain &c, const double tol,
                       const int max_iter, const unsigned int verbose, bool useHessian) :
                       chain(c), verbosity(verbose)
{
    pLIC=&noLIC;

    chain.setAllConstraints(false); // this is required since IpOpt initially relaxes constraints

    App=new IpoptApplication();

    CAST_IPOPTAPP(App)->Options()->SetNumericValue("tol",tol);
    CAST_IPOPTAPP(App)->Options()->SetNumericValue("acceptable_tol",tol);
    CAST_IPOPTAPP(App)->Options()->SetIntegerValue("acceptable_iter",10);
    CAST_IPOPTAPP(App)->Options()->SetStringValue("mu_strategy","adaptive");
    CAST_IPOPTAPP(App)->Options()->SetIntegerValue("print_level",verbose);

    // CAST_IPOPTAPP(App)->Options()->SetStringValue("jacobian_approximation","finite-difference-values");
    CAST_IPOPTAPP(App)->Options()->SetStringValue("nlp_scaling_method","gradient-based");
    CAST_IPOPTAPP(App)->Options()->SetStringValue("derivative_test","none");
    // CAST_IPOPTAPP(App)->Options()->SetStringValue("derivative_test","first-order");
    CAST_IPOPTAPP(App)->Options()->SetStringValue("derivative_test_print_all","yes");
    // CAST_IPOPTAPP(App)->Options()->SetStringValue("print_timing_statistics","yes");
    // CAST_IPOPTAPP(App)->Options()->SetStringValue("print_options_documentation","no");

    getBoundsInf(lowerBoundInf,upperBoundInf);

    if (max_iter>0)
        CAST_IPOPTAPP(App)->Options()->SetIntegerValue("max_iter",max_iter);
    else
        CAST_IPOPTAPP(App)->Options()->SetIntegerValue("max_iter",(Index)upperBoundInf);

    if (!useHessian)
        CAST_IPOPTAPP(App)->Options()->SetStringValue("hessian_approximation","limited-memory");

    Ipopt::ApplicationReturnStatus status = CAST_IPOPTAPP(App)->Initialize();
    if (status != Ipopt::Solve_Succeeded) {
        yError("Error during initialization!");
    }
}

/************************************************************************/
void reactIpOpt::setMaxIter(const int max_iter)
{
    if (max_iter>0)
        CAST_IPOPTAPP(App)->Options()->SetIntegerValue("max_iter",max_iter);
    else
        CAST_IPOPTAPP(App)->Options()->SetIntegerValue("max_iter",std::numeric_limits<int>::max());

    CAST_IPOPTAPP(App)->Initialize();
}


/************************************************************************/
int reactIpOpt::getMaxIter() const
{
    int max_iter;
    CAST_IPOPTAPP(App)->Options()->GetIntegerValue("max_iter",max_iter,"");
    return max_iter;
}


/************************************************************************/
void reactIpOpt::setTol(const double tol)
{
    CAST_IPOPTAPP(App)->Options()->SetNumericValue("tol",tol);
    CAST_IPOPTAPP(App)->Options()->SetNumericValue("acceptable_tol",tol);

    CAST_IPOPTAPP(App)->Initialize();
}


/************************************************************************/
double reactIpOpt::getTol() const
{
    double tol;
    CAST_IPOPTAPP(App)->Options()->GetNumericValue("tol",tol,"");
    return tol;
}


/************************************************************************/
void reactIpOpt::setVerbosity(const unsigned int verbose)
{
    CAST_IPOPTAPP(App)->Options()->SetIntegerValue("print_level",verbose);

    CAST_IPOPTAPP(App)->Initialize();
}


/************************************************************************/
void reactIpOpt::setUserScaling(const bool useUserScaling, const double _obj_scaling,
                                  const double _x_scaling, const double _g_scaling)
{
    if (useUserScaling)
    {
        obj_scaling=_obj_scaling;
        x_scaling  =_x_scaling;
        g_scaling  =_g_scaling;

        CAST_IPOPTAPP(App)->Options()->SetStringValue("nlp_scaling_method","user-scaling");
    }
    else
        CAST_IPOPTAPP(App)->Options()->SetStringValue("nlp_scaling_method","gradient-based");

    CAST_IPOPTAPP(App)->Initialize();
}


/************************************************************************/
void reactIpOpt::getBoundsInf(double &lower, double &upper)
{
    CAST_IPOPTAPP(App)->Options()->GetNumericValue("nlp_lower_bound_inf",lower,"");
    CAST_IPOPTAPP(App)->Options()->GetNumericValue("nlp_upper_bound_inf",upper,"");
}


/************************************************************************/
void reactIpOpt::setBoundsInf(const double lower, const double upper)
{
    CAST_IPOPTAPP(App)->Options()->SetNumericValue("nlp_lower_bound_inf",lower);
    CAST_IPOPTAPP(App)->Options()->SetNumericValue("nlp_upper_bound_inf",upper);

    lowerBoundInf=lower;
    upperBoundInf=upper;
}


/************************************************************************/
yarp::sig::Vector reactIpOpt::solve(yarp::sig::Vector &xd, yarp::sig::Vector q_dot_0,
                                    double &dt, double *cpu_time,int *exit_code)
{
    SmartPtr<react_NLP> nlp=new react_NLP(chain,xd,q_dot_0,dt,*pLIC,verbosity);
    
    nlp->set_scaling(obj_scaling,x_scaling,g_scaling);
    nlp->set_bound_inf(lowerBoundInf,upperBoundInf);

    CAST_IPOPTAPP(App)->Options()->SetNumericValue("max_cpu_time",dt);
    ApplicationReturnStatus status=CAST_IPOPTAPP(App)->OptimizeTNLP(GetRawPtr(nlp));

    if (exit_code!=NULL)
        *exit_code=status;

    // if (cpu_time!=NULL)
    //     *cpu_time=

    return nlp->get_q_dot_d();
}

/************************************************************************/
reactIpOpt::~reactIpOpt()
{
    delete CAST_IPOPTAPP(App);
}

