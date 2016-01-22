/***************************************************
  A simple dynamic filter library

  Class code for embedded application.

  07-Jan-2015   Dave Gutz   Created
 ****************************************************/

#ifndef _myFilters_H
#define _myFilters_H


class DiscreteFilter
{
public:
  DiscreteFilter();
  DiscreteFilter(const double T, const double tau, const double min, const double max);
  virtual ~DiscreteFilter();
  // operators
  // functions
  virtual double  calculate(double in, int RESET);
  virtual void    assignCoeff(double tau);
  virtual void    rateState(double in);
  virtual double  state(void);
protected:
  double max_;
  double min_;
  double rate_;
  double T_;
  double tau_;
};

// Tustin rate-lag rate calculator, non-pre-warped, no limits, fixed update rate
class RateLagTustin: public DiscreteFilter
{
public:
  RateLagTustin();
  RateLagTustin(const double T, const double tau, const double min, const double max);
//  RateLagTustin(const RateLagTustin & RLT);
  ~RateLagTustin();
  //operators
  //functions
  virtual double  calculate(double in, int RESET);
  virtual void    assignCoeff(double tau);
  virtual void    rateState(double in);
  virtual double  state(void);
protected:
  double a_;
  double b_;
  double state_;
};


// Exponential rate-lag rate calculator
class RateLagExp: public DiscreteFilter
{
public:
  RateLagExp();
  RateLagExp(const double T, const double tau, const double min, const double max);
  //RateLagExp(const RateLagExp & RLT);
  ~RateLagExp();
  //operators
  //functions
  virtual double  calculate(double in, int RESET);
  virtual double  calculate(double in, int RESET, const double T);
  virtual void    assignCoeff(double tau);
  virtual void    rateState(double in);
  virtual void    rateState(double in, const double T);
  virtual double  state(void);
  double a(){return(a_);};
  double b(){return(b_);};
  double c(){return(c_);};
  double lstate(){return(lstate_);};
  double rstate(){return(rstate_);};
protected:
  double a_;
  double b_;
  double c_;
  double lstate_;   // lag state
  double rstate_;   // rate state
};


#endif
