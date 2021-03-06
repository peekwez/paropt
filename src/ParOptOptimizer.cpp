#include <string.h>
#include "ParOptOptimizer.h"


ParOptOptimizer::ParOptOptimizer( ParOptProblem *_problem,
                   ParOptOptions *_options ){
  problem = _problem;
  problem->incref();

  options = _options;
  options->incref();

  ip = NULL;
  tr = NULL;
  mma = NULL;
  subproblem = NULL;
}

ParOptOptimizer::~ParOptOptimizer(){
  problem->decref();
  options->decref();
  if (ip){ ip->decref(); }
  if (tr){ tr->decref(); }
  if (mma){ mma->decref(); }
  if (subproblem){ subproblem->decref(); }
}

/*
  Get default optimization options
*/
void ParOptOptimizer::addDefaultOptions( ParOptOptions *options ){
  const char *optimizers[3] = {"ip", "tr", "mma"};
  options->addEnumOption("algorithm", "tr", 3, optimizers,
    "The type of optimization algorithm");

  options->addStringOption("ip_checkpoint_file", NULL,
    "Checkpoint file for the interior point method");

  ParOptInteriorPoint::addDefaultOptions(options);
  ParOptTrustRegion::addDefaultOptions(options);
  ParOptMMA::addDefaultOptions(options);
}

/*
  Get the options set into the optimizer
*/
ParOptOptions *ParOptOptimizer::getOptions(){
  return options;
}

/*
  Get the problem class
*/
ParOptProblem* ParOptOptimizer::getProblem(){
  return problem;
}

/*
  Perform the optimization
*/
void ParOptOptimizer::optimize(){
  // Check what type of optimization algorithm has been requirested
  int algo_type = 0;
  const char *algorithm = options->getEnumOption("algorithm");

  if (strcmp(algorithm, "ip") == 0){
    algo_type = 1;
  }
  else if (strcmp(algorithm, "tr") == 0){
    algo_type = 2;
  }
  else if (strcmp(algorithm, "mma") == 0){
    algo_type = 3;
  }
  else {
    fprintf(stderr, "ParOptOptimizer Error: Unrecognized algorithm option %s\n",
            algorithm);
    return;
  }

  if (algo_type == 1){
    if (tr && ip){
      tr->decref();
      tr = NULL;
      ip->decref();
      ip = NULL;
    }
    else if (mma && ip){
      mma->decref();
      mma = NULL;
      ip->decref();
      ip = NULL;
    }

    if (!ip){
      ip = new ParOptInteriorPoint(problem, options);
      ip->incref();
    }

    const char *checkpoint = options->getStringOption("ip_checkpoint_file");
    ip->optimize(checkpoint);
  }
  else if (algo_type == 2){
    if (mma && ip){
      mma->decref();
      mma = NULL;
      ip->decref();
      ip = NULL;
    }

    // Create the trust region subproblem
    const char *qn_type = options->getEnumOption("qn_type");
    const int qn_subspace_size = options->getIntOption("qn_subspace_size");

    if (!subproblem){
      ParOptCompactQuasiNewton *qn = NULL;
      if (strcmp(qn_type, "bfgs") == 0){
        qn = new ParOptLBFGS(problem, qn_subspace_size);
        qn->incref();
      }
      else if (strcmp(qn_type, "sr1") == 0){
        qn = new ParOptLSR1(problem, qn_subspace_size);
        qn->incref();
      }

      subproblem = new ParOptQuadraticSubproblem(problem, qn);
      subproblem->incref();
    }

    if (!ip){
      ip = new ParOptInteriorPoint(subproblem, options);
      ip->incref();
    }

    if (!tr){
      tr = new ParOptTrustRegion(subproblem, options);
      tr->incref();
    }

    tr->optimize(ip);
  }
  else { // algo_type == 3
    // Set up the MMA optimizer
    if (tr && ip){
      tr->decref();
      tr = NULL;
      ip->decref();
      ip = NULL;
    }

    // Create the the mma object
    if (!mma){
      mma = new ParOptMMA(problem, options);
      mma->incref();
    }

    if (!ip){
      ip = new ParOptInteriorPoint(mma, options);
      ip->incref();
    }

    mma->optimize(ip);
  }
}

// Get the optimized point
void ParOptOptimizer::getOptimizedPoint( ParOptVec **x,
                                         ParOptScalar **z,
                                         ParOptVec **zw,
                                         ParOptVec **zl,
                                         ParOptVec **zu ){
  if (tr && ip){
    tr->getOptimizedPoint(x);
    ip->getOptimizedPoint(NULL, z, zw, zl, zu);
  }
  else if (mma && ip){
    mma->getOptimizedPoint(x);
    ip->getOptimizedPoint(NULL, z, zw, zl, zu);
  }
  else if (ip){
    ip->getOptimizedPoint(x, z, zw, zl, zu);
  }
}

/*
  Set the trust-region subproblem
*/
void ParOptOptimizer::setTrustRegionSubproblem( ParOptTrustRegionSubproblem *_subproblem ){
  // Should check here if the subproblem's problem
  // is our problem, for consistency??
  if (_subproblem){
    _subproblem->incref();
  }
  if (subproblem){
    subproblem->decref();
  }
  subproblem = _subproblem;
}
