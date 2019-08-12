// [[Rcpp::depends(RcppArmadillo)]]

#include <RcppArmadillo.h>
#include <math.h>
#include <unordered_set>
#include <string>
#include <algorithm>
#include <stack>

using namespace Rcpp;
using namespace std;
using namespace arma;

template <typename T>
T sortByDimNames(const T m);


// check if two vectors are intersected
bool intersects(CharacterVector x, CharacterVector y) {
  if (x.size() < y.size())
    return intersects(y, x);
  else {
    unordered_set<string> values;
    bool intersect = false;
    
    for (auto value : x)
      values.insert(as<string>(value));
    
    for (auto it = y.begin(); it != y.end() && !intersect; ++it)
      intersect = values.count(as<string>(*it)) > 0;
    
    return intersect;
  }
}


// [[Rcpp::export(.commClassesKernelRcpp)]]
List commClassesKernel(NumericMatrix P) {
  // The matrix must be stochastic by rows
  unsigned int numStates = P.ncol();
  CharacterVector stateNames = rownames(P);
  int numReachable;
  int classSize;
  
  // The entry (i,j) of this matrix is true iff we can reach j from i
  vector<vector<bool>> communicates(numStates, vector<bool>(numStates, false));
  vector<list<int>> adjacencies(numStates);
  
  // We fill the adjacencies matrix for the graph
  // A state j is in the adjacency of i iff P(i, j) > 0
  for (int i = 0; i < numStates; ++i)
    for (int j = 0; j < numStates; ++j)
      if (P(i, j) > 0)
        adjacencies[i].push_back(j);


  // Backtrack from all the states to find which
  // states communicate with a given on
  // O(n³) where n is the number of states
  for (int i = 0; i < numStates; ++i) {
    stack<int> notVisited;
    notVisited.push(i);
    
    while (!notVisited.empty()) {
      int j = notVisited.top();
      notVisited.pop();
      communicates[i][j] = true;
      
      for (int k: adjacencies[j])
        if (!communicates[i][k])
          notVisited.push(k);
    }
  }
  
  LogicalMatrix classes(numStates, numStates);
  classes.attr("dimnames") = List::create(stateNames, stateNames);
  // v populated with FALSEs
  LogicalVector closed(numStates);
  closed.names() = stateNames;
  
  for (int i = 0; i < numStates; ++i) {
    numReachable = 0;
    classSize = 0;
    
    /* We mark i and j as the same communicating class iff we can reach the
       state j from i and the state i from j
       We count the size of the communicating class of i (i is fixed here),
       and if it matches the number of states that can be reached from i,
       then the class is closed
    */
    for (int j = 0; j < numStates; ++j) {
      classes(i, j) = communicates[i][j] && communicates[j][i];
      
      if (classes(i,j))
        classSize += 1;

      // Number of states reachable from i
      if (communicates[i][j])
        numReachable += 1;
    }
    
    if (classSize == numReachable)
      closed(i) = true;
  }
  
  return List::create(_["classes"] = classes, _["closed"] = closed);
}

// Wrapper that computes the communicating states from the matrix generated by 
// commClassesKernel (a matrix where an entry i,j is TRUE iff i and j are in the
// same communicating class). It also needs the list of states names from the
// Markov Chain
List computeCommunicatingClasses(LogicalMatrix& commClasses, CharacterVector& states) {
  int numStates = states.size();
  vector<bool> computed(numStates, false);
  List classesList;
  
  for (int i = 0; i < numStates; ++i) {
    CharacterVector currentClass;
    
    if (!computed[i]) {
      for (int j = 0; j < numStates; ++j) {
        if (commClasses(i, j)) {
          currentClass.push_back(states[j]);
          computed[j] = true;
        }
      }
      
      classesList.push_back(currentClass);
    }
  }
  
  return classesList;
}

// [[Rcpp::export(.communicatingClassesRcpp)]]
List communicatingClasses(S4 object) {
  // Returns the underlying communicating classes
  // It is indifferent if the matrices are stochastic by rows or columns
  NumericMatrix transitionMatrix = object.slot("transitionMatrix");
  bool byrow = object.slot("byrow");
  CharacterVector states = object.slot("states");
  
  if (!byrow)
    transitionMatrix = transpose(transitionMatrix);
  
  List commClassesList = commClassesKernel(transitionMatrix);
  LogicalMatrix commClasses = commClassesList["classes"];
  
  return computeCommunicatingClasses(commClasses, states);
}

// Wrapper that computes the transient states from a list of the states and a
// vector indicating whether the communicating class for each state is closed
CharacterVector computeTransientStates(CharacterVector& states, LogicalVector& closedClass) {
  CharacterVector transientStates;
  
  for (int i = 0; i < states.size(); i++)
    if (!closedClass[i])
      transientStates.push_back(states[i]);
    
  return transientStates;
}

// Wrapper that computes the recurrent states from a list of states and a
// vector indicating whether the communicating class for each state is closed
CharacterVector computeRecurrentStates(CharacterVector& states, LogicalVector& closedClass) {
  CharacterVector recurrentStates;
  
  for (int i = 0; i < states.size(); i++)
    if (closedClass[i])
      recurrentStates.push_back(states[i]);
    
  return recurrentStates;
}

// [[Rcpp::export(.transientStatesRcpp)]]
CharacterVector transientStates(S4 object) {
  NumericMatrix transitionMatrix = object.slot("transitionMatrix");
  bool byrow = object.slot("byrow");
  
  if (!byrow)
    transitionMatrix = transpose(transitionMatrix);
  
  List commKernel = commClassesKernel(transitionMatrix);
  LogicalVector closed = commKernel["closed"];
  CharacterVector states = object.slot("states");

  return computeTransientStates(states, closed);
}

// [[Rcpp::export(.recurrentStatesRcpp)]]
CharacterVector recurrentStates(S4 object) {
  NumericMatrix transitionMatrix = object.slot("transitionMatrix");
  bool byrow = object.slot("byrow");
  CharacterVector states = object.slot("states");
  
  if (!byrow)
    transitionMatrix = transpose(transitionMatrix);
  
  List commKernel = commClassesKernel(transitionMatrix);
  LogicalVector closed = commKernel["closed"];
  
  return computeRecurrentStates(states, closed);
}

// Wrapper that computes the recurrent classes from the matrix given by 
// commClassesKernel (which entry i,j is TRUE iff i and j are in the same
// communicating class), a vector indicating wheter the class for state is
// closed and the states of the Markov Chain
List computeRecurrentClasses(LogicalMatrix& commClasses, 
                             LogicalVector& closedClass, 
                             CharacterVector& states) {
  int numStates = states.size();
  vector<bool> computed(numStates, false);
  List recurrentClassesList;
  bool isRecurrentClass;
  
  for (int i = 0; i < numStates; ++i) {
    CharacterVector currentClass;
    isRecurrentClass = closedClass(i) && !computed[i];
    
    if (isRecurrentClass) {
      for (int j = 0; j < numStates; ++j) {
        if (commClasses(i, j)) {
          currentClass.push_back(states[j]);
          computed[j] = true;
        }
      }
      
      recurrentClassesList.push_back(currentClass);
    }
  }
  
  return recurrentClassesList;
}

// returns the recurrent classes
// [[Rcpp::export(.recurrentClassesRcpp)]]
List recurrentClasses(S4 object) {
  NumericMatrix transitionMatrix = object.slot("transitionMatrix");
  bool byrow = object.slot("byrow");
  CharacterVector states = object.slot("states");
  
  if (!byrow)
    transitionMatrix = transpose(transitionMatrix);
  
  List commClassesList = commClassesKernel(transitionMatrix);
  LogicalMatrix commClasses = commClassesList["classes"];
  LogicalVector closed = commClassesList["closed"];
  
  return computeRecurrentClasses(commClasses, closed, states);
}

// Wrapper that computes the transient classes from the matrix given by 
// commClassesKernel (which entry i,j is TRUE iff i and j are in the same
// communicating class), a vector indicating wheter the class for state is
// closed and the states of the Markov Chain
List computeTransientClasses(LogicalMatrix& commClasses, 
                             LogicalVector& closedClass, 
                             CharacterVector& states) {
  int numStates = states.size();
  vector<bool> computed(numStates, false);
  List transientClassesList;
  bool isTransientClass;
  
  for (int i = 0; i < numStates; ++i) {
    CharacterVector currentClass;
    isTransientClass = !closedClass(i) && !computed[i];
    
    if (isTransientClass) {
      for (int j = 0; j < numStates; ++j) {
        if (commClasses(i, j)) {
          currentClass.push_back(states[j]);
          computed[j] = true;
        }
      }
      
      transientClassesList.push_back(currentClass);
    }
  }
  
  return transientClassesList;
}

// returns the transient classes
// [[Rcpp::export(.transientClassesRcpp)]]
List transientClasses(S4 object) {
  NumericMatrix transitionMatrix = object.slot("transitionMatrix");
  bool byrow = object.slot("byrow");
  CharacterVector states = object.slot("states");
  
  if (!byrow)
    transitionMatrix = transpose(transitionMatrix);
  
  List commClassesList = commClassesKernel(transitionMatrix);
  LogicalMatrix commClasses = commClassesList["classes"];
  LogicalVector closed = commClassesList["closed"];
  
  return computeTransientClasses(commClasses, closed, states);
}

// matrix power function
arma::mat _pow(arma::mat A, int n) {
  arma::mat R = arma::eye(A.n_rows, A.n_rows);
  
  for (int i = 0; i < n; i ++) 
    R = A*R;
  
  return R;
}

//communicating states
// [[Rcpp::export(.commStatesFinderRcpp)]]
NumericMatrix commStatesFinder(NumericMatrix matr) {
  //Reachability matrix
  int dimMatr = matr.nrow();
  arma::mat X(matr.begin(), dimMatr, dimMatr, false);
  arma::mat temp = arma::eye(dimMatr, dimMatr) + arma::sign(X);
  temp = _pow(temp, dimMatr - 1);
  NumericMatrix R = wrap(arma::sign(temp));
  R.attr("dimnames") = matr.attr("dimnames");
  
  return R;
}

template<class T>
T efficientPow(T a, T identity, T (*product)(const T&, const T&), T (*sum)(const T&, const T&), int n) {
  T result  = identity;
  T partial = identity;
  
  // We can decompose n = 2^a + 2^b + 2^c ... with a > b > c >= 0
  // Compute last = a + 1
  while (n > 0) {
    if (n & 1 > 0)
      result = sum(result, partial);
    
    partial = product(partial, partial);
    n >>= 1;
  }
  
  return result;
}

// summary of markovchain object
// [[Rcpp::export(.summaryKernelRcpp)]]
List summaryKernel(S4 object) {
  NumericMatrix transitionMatrix = object.slot("transitionMatrix");
  bool byrow = object.slot("byrow");
  CharacterVector states = object.slot("states");
  
  if (!byrow)
    transitionMatrix = transpose(transitionMatrix);
  
  List commClassesList = commClassesKernel(transitionMatrix);
  LogicalMatrix commClasses = commClassesList["classes"];
  LogicalVector closed = commClassesList["closed"];
  List recurrentClasses = computeRecurrentClasses(commClasses, closed, states);
  List transientClasses = computeTransientClasses(commClasses, closed, states);
  
  List summaryResult = List::create(_["closedClasses"]    = recurrentClasses,
                                    _["recurrentClasses"] = recurrentClasses,
                                    _["transientClasses"] = transientClasses);
  
  return(summaryResult);
}

//here the kernel function to compute the first passage
// [[Rcpp::export(.firstpassageKernelRcpp)]]
NumericMatrix firstpassageKernel(NumericMatrix P, int i, int n) {
  arma::mat G = as<arma::mat>(P);
  arma::mat Pa = G;
  arma::mat H(n, P.ncol()); 
  
  //here Thoralf suggestion
  //initializing the first row
  for (unsigned int j = 0; j < G.n_cols; j++)
    H(0, j) = G(i-1, j);
  
  arma::mat E = 1 - arma::eye(P.ncol(), P.ncol());

  for (int m = 1; m < n; m++) {
    G = Pa * (G%E);
    
    for (unsigned int j = 0; j < G.n_cols; j ++) 
      H(m, j) = G(i-1, j);
  }
  
  NumericMatrix R = wrap(H);
  
  return R;
}



// [[Rcpp::export(.firstPassageMultipleRCpp)]]
NumericVector firstPassageMultipleRCpp(NumericMatrix P,int i, NumericVector setno, int n) {
  arma::mat G = as<arma::mat>(P);
  arma::mat Pa = G;
  arma::vec H = arma::zeros(n); //here Thoralf suggestion
  unsigned int size = setno.size();
  //initializing the first row
  for (unsigned int k = 0; k < size; k++) {
    H[0] += G(i-1, setno[k]-1);
  }
  
  arma::mat E = 1 - arma::eye(P.ncol(), P.ncol());
  
  for (int m = 1; m < n; m++) {
    G = Pa * (G%E);
    
    for (unsigned int k = 0; k < size; k++) {
      H[m] += G(i-1, setno[k]-1);
    }
  }
  
  NumericVector R = wrap(H);
  
  return R;
}

// [[Rcpp::export(.expectedRewardsRCpp)]]
NumericVector expectedRewardsRCpp(NumericMatrix matrix, int n, NumericVector rewards) {
  // initialises output vector
  NumericVector out;
  
  // gets no of states
  int no_of_states = matrix.ncol();
  
  // initialises armadillo matrices and vectors
  arma::vec temp = arma::zeros(no_of_states);
  arma::mat matr = as<arma::mat>(matrix);
  arma::vec v = arma::zeros(no_of_states);
  
  // initialses the vector for the base case of dynamic programming expression
  for (int i=0;i<no_of_states;i++) {
    temp[i] = rewards[i];
    v[i] = rewards[i];
  }
  
  // v(n, u) = r + [P]v(n−1, u);
  for (int i=0;i<n;i++) {
    temp = v + matr*temp;
  }
  
  // gets output in form of NumericVector
  out = wrap(temp);
  
  return out;
}

// [[Rcpp::export(.expectedRewardsBeforeHittingARCpp)]]
double expectedRewardsBeforeHittingARCpp(NumericMatrix matrix,int s0,
                               NumericVector rewards, int n ) {
  float result = 0.0;
  int size = rewards.size();
  arma::mat matr = as<arma::mat>(matrix);
  arma::mat temp = as<arma::mat>(matrix);
  arma::vec r = as<arma::vec>(rewards);
  arma::mat I = arma::zeros(1,size);
  
  I(0,s0-1) = 1;
  
  for (int j = 0; j < n; j++) {
    arma::mat res = I*(temp*r);
    result = result + res(0,0);
    temp = temp*matr;
  }
  
  return result;
}
  

// greatest common denominator
// [[Rcpp::export(.gcdRcpp)]]
int gcd (int a, int b) {
  int c;
  a = abs(a);
  b = abs(b);

  while ( a != 0 ) {
    c = a; a = b%a;  b = c;
  }
  
  return b;
}

// function to get the period of a DTMC

//' @rdname structuralAnalysis
//' 
//' @export
// [[Rcpp::export(period)]]
int period(S4 object) {
  Function isIrreducible("is.irreducible");
  List res = isIrreducible(object);
  
  if (!res[0]) {
    warning("The matrix is not irreducible");
    return 0;
  } else {
    NumericMatrix P = object.slot("transitionMatrix");
    int n = P.ncol();
    std::vector<double> r, T(1), w;
    int d = 0, m = T.size(), i = 0, j = 0;
    
    if (n > 0) {
      arma::vec v(n);
      v[0] = 1;
      
      while (m>0 && d!=1) {
        i = T[0];
        T.erase(T.begin());
        w.push_back(i);
        j = 0;
        
        while (j < n) {
          if (P(i,j) > 0) {
            r.insert(r.end(), w.begin(), w.end());
            r.insert(r.end(), T.begin(), T.end());
            double k = 0;
            
            for (std::vector<double>::iterator it = r.begin(); it != r.end(); it ++) 
              if (*it == j) k ++;
            
            if (k > 0) {
               int b = v[i] + 1 - v[j];
               d = gcd(d, b);
            } else {
              T.push_back(j);
              v[j] = v[i] + 1;
            }
          }
          j++;
        }
        m = T.size();
      }
    }
    
    // v = v - floor(v/d)*d;
    return d;
  }
}

//' @title predictiveDistribution
//'
//' @description The function computes the probability of observing a new data
//'   set, given a data set
//' @usage predictiveDistribution(stringchar, newData, hyperparam = matrix())
//'
//' @param stringchar This is the data using which the Bayesian inference is
//'   performed.
//' @param newData This is the data whose predictive probability is computed.
//' @param hyperparam This determines the shape of the prior distribution of the
//'   parameters. If none is provided, default value of 1 is assigned to each
//'   parameter. This must be of size kxk where k is the number of states in the
//'   chain and the values should typically be non-negative integers.
//' @return The log of the probability is returned.
//'
//' @details The underlying method is Bayesian inference. The probability is
//'   computed by averaging the likelihood of the new data with respect to the
//'   posterior. Since the method assumes conjugate priors, the result can be
//'   represented in a closed form (see the vignette for more details), which is
//'   what is returned.
//' @references 
//' Inferring Markov Chains: Bayesian Estimation, Model Comparison, Entropy Rate, 
//' and Out-of-Class Modeling, Christopher C. Strelioff, James P.
//' Crutchfield, Alfred Hubler, Santa Fe Institute
//' 
//' Yalamanchi SB, Spedicato GA (2015). Bayesian Inference of First Order Markov 
//' Chains. R package version 0.2.5
//' 
//' @author Sai Bhargav Yalamanchi
//' @seealso \code{\link{markovchainFit}}
//' @examples
//' sequence<- c("a", "b", "a", "a", "a", "a", "b", "a", "b", "a", "b", "a", "a", 
//'              "b", "b", "b", "a")
//' hyperMatrix<-matrix(c(1, 2, 1, 4), nrow = 2,dimnames=list(c("a","b"),c("a","b")))
//' predProb <- predictiveDistribution(sequence[1:10], sequence[11:17], hyperparam =hyperMatrix )
//' hyperMatrix2<-hyperMatrix[c(2,1),c(2,1)]
//' predProb2 <- predictiveDistribution(sequence[1:10], sequence[11:17], hyperparam =hyperMatrix2 )
//' predProb2==predProb
//' @export
//' 
// [[Rcpp::export]]
double predictiveDistribution(CharacterVector stringchar, CharacterVector newData, NumericMatrix hyperparam = NumericMatrix()) {
  // construct list of states
  CharacterVector elements = stringchar;
  
  for (int i = 0; i < newData.size(); i++)
    elements.push_back(newData[i]);
  
  elements = unique(elements).sort();
  int sizeMatr = elements.size();
  
  // if no hyperparam argument provided, use default value of 1 for all 
  if (hyperparam.nrow() == 1 && hyperparam.ncol() == 1) {
    NumericMatrix temp(sizeMatr, sizeMatr);
    temp.attr("dimnames") = List::create(elements, elements);
    
    for (int i = 0; i < sizeMatr; i++)
      for (int j = 0; j < sizeMatr; j++)
        temp(i, j) = 1;
    
    hyperparam = temp;
  }
  
  // validity check
  if (hyperparam.nrow() != hyperparam.ncol())
    stop("Dimensions of the hyperparameter matrix are inconsistent");
    
  if (hyperparam.nrow() < sizeMatr)
    stop("Hyperparameters for all state transitions must be provided");
    
  List dimNames = hyperparam.attr("dimnames");
  CharacterVector colNames = dimNames[1];
  CharacterVector rowNames = dimNames[0];
  int sizeHyperparam = hyperparam.ncol();
  CharacterVector sortedColNames(sizeHyperparam), sortedRowNames(sizeHyperparam);
  
  for (int i = 0; i < sizeHyperparam; i++)
    sortedColNames(i) = colNames(i), sortedRowNames(i) = rowNames(i);

  sortedColNames.sort();
  sortedRowNames.sort();
  
  for (int i = 0; i < sizeHyperparam; i++) {
    if (i > 0 && (sortedColNames(i) == sortedColNames(i-1) || sortedRowNames(i) == sortedRowNames(i-1)))  
      stop("The states must all be unique");
    else if (sortedColNames(i) != sortedRowNames(i))
      stop("The set of row names must be the same as the set of column names");
    
    bool found = false;
    
    for (int j = 0; j < sizeMatr; j++)
      if (elements(j) == sortedColNames(i))
        found = true;
    // hyperparam may contain states not in stringchar
    if (!found)  elements.push_back(sortedColNames(i));
  }
  
  // check for the case where hyperparam has missing data
  for (int i = 0; i < sizeMatr; i++) {
    bool found = false;
    
    for (int j = 0; j < sizeHyperparam; j++)
      if (sortedColNames(j) == elements(i))
        found = true;
    
    if (!found)
      stop("Hyperparameters for all state transitions must be provided");
  }   
      
  elements = elements.sort();
  sizeMatr = elements.size();
  
  for (int i = 0; i < sizeMatr; i++)
    for (int j = 0; j < sizeMatr; j++)
      if (hyperparam(i, j) < 1.)
        stop("The hyperparameter elements must all be greater than or equal to 1");
  
  // permute the elements of hyperparam such that the row, column names are sorted
  hyperparam = sortByDimNames(hyperparam);
  
  NumericMatrix freqMatr(sizeMatr), newFreqMatr(sizeMatr);

  double predictiveDist = 0.; // log of the predictive probability

  // populate frequeny matrix for old data; this is used for inference 
  int posFrom = 0, posTo = 0;
  
  for (int i = 0; i < stringchar.size() - 1; i ++) {
    for (int j = 0; j < sizeMatr; j ++) {
      if (stringchar[i] == elements[j]) posFrom = j;
      if (stringchar[i + 1] == elements[j]) posTo = j;
    }
    freqMatr(posFrom,posTo)++;
  }
  
  // frequency matrix for new data
  for (int i = 0; i < newData.size() - 1; i ++) {
    for (int j = 0; j < sizeMatr; j ++) {
      if (newData[i] == elements[j]) posFrom = j;
      if (newData[i + 1] == elements[j]) posTo = j;
    }
    newFreqMatr(posFrom,posTo)++;
  }
 
  for (int i = 0; i < sizeMatr; i++) {
    double rowSum = 0, newRowSum = 0, paramRowSum = 0;
    
    for (int j = 0; j < sizeMatr; j++) { 
      rowSum += freqMatr(i, j), newRowSum += newFreqMatr(i, j), paramRowSum += hyperparam(i, j);
      predictiveDist += lgamma(freqMatr(i, j) + newFreqMatr(i, j) + hyperparam(i, j)) -
                        lgamma(freqMatr(i, j) + hyperparam(i, j));
    }
    predictiveDist += lgamma(rowSum + paramRowSum) - lgamma(rowSum + newRowSum + paramRowSum);
  }

  return predictiveDist;
}


//' @title priorDistribution
//'
//' @description Function to evaluate the prior probability of a transition
//'   matrix. It is based on conjugate priors and therefore a Dirichlet
//'   distribution is used to model the transitions of each state.
//' @usage priorDistribution(transMatr, hyperparam = matrix())
//'
//' @param transMatr The transition matrix whose probability is the parameter of
//'   interest.
//' @param hyperparam The hyperparam matrix (optional). If not provided, a
//'   default value of 1 is assumed for each and therefore the resulting
//'   probability distribution is uniform.
//' @return The log of the probabilities for each state is returned in a numeric
//'   vector. Each number in the vector represents the probability (log) of
//'   having a probability transition vector as specified in corresponding the
//'   row of the transition matrix.
//'
//' @details The states (dimnames) of the transition matrix and the hyperparam
//'   may be in any order.
//' @references Yalamanchi SB, Spedicato GA (2015). Bayesian Inference of First
//' Order Markov Chains. R package version 0.2.5
//'
//' @author Sai Bhargav Yalamanchi, Giorgio Spedicato
//'
//' @note This function can be used in conjunction with inferHyperparam. For
//'   example, if the user has a prior data set and a prior transition matrix,
//'   he can infer the hyperparameters using inferHyperparam and then compute
//'   the probability of their prior matrix using the inferred hyperparameters
//'   with priorDistribution.
//' @seealso \code{\link{predictiveDistribution}}, \code{\link{inferHyperparam}}
//' 
//' @examples
//' priorDistribution(matrix(c(0.5, 0.5, 0.5, 0.5), 
//'                   nrow = 2, 
//'                   dimnames = list(c("a", "b"), c("a", "b"))), 
//'                   matrix(c(2, 2, 2, 2), 
//'                   nrow = 2, 
//'                   dimnames = list(c("a", "b"), c("a", "b"))))
//' @export
// [[Rcpp::export]]
NumericVector priorDistribution(NumericMatrix transMatr, NumericMatrix hyperparam = NumericMatrix()) {
  // begin validity checks for the transition matrix
  if (transMatr.nrow() != transMatr.ncol())
    stop("Transition matrix dimensions are inconsistent");
    
  int sizeMatr = transMatr.nrow();
  
  for (int i = 0; i < sizeMatr; i++) {
    double rowSum = 0., eps = 1e-10;
    
    for (int j = 0; j < sizeMatr; j++)
      if (transMatr(i, j) < 0. || transMatr(i, j) > 1.)
        stop("The entries in the transition matrix must each belong to the interval [0, 1]");
      else
        rowSum += transMatr(i, j);
    
    if (rowSum <= 1. - eps || rowSum >= 1. + eps)
      stop("The rows of the transition matrix must each sum to 1");
  }
  
  List dimNames = transMatr.attr("dimnames");
  
  if (dimNames.size() == 0)
    stop("Provide dimnames for the transition matrix");
  
  CharacterVector colNames = dimNames[1];
  CharacterVector rowNames = dimNames[0];
  CharacterVector sortedColNames(sizeMatr), sortedRowNames(sizeMatr);
  
  for (int i = 0; i < sizeMatr; i++)
    sortedColNames(i) = colNames(i), sortedRowNames(i) = rowNames(i);
  
  sortedColNames.sort();
  sortedRowNames.sort();
  
  for (int i = 0; i < sizeMatr; i++) 
    if (i > 0 && (sortedColNames(i) == sortedColNames(i-1) || sortedRowNames(i) == sortedRowNames(i-1)))  
      stop("The states must all be unique");
    else if (sortedColNames(i) != sortedRowNames(i))
      stop("The set of row names must be the same as the set of column names");
  
  // if no hyperparam argument provided, use default value of 1 for all 
  if (hyperparam.nrow() == 1 && hyperparam.ncol() == 1) {
    NumericMatrix temp(sizeMatr, sizeMatr);
    temp.attr("dimnames") = List::create(sortedColNames, sortedColNames);
  
    for (int i = 0; i < sizeMatr; i++)
      for (int j = 0; j < sizeMatr; j++)
        temp(i, j) = 1;
  
    hyperparam = temp;
  }
  
  // validity check for hyperparam
  if (hyperparam.nrow() != hyperparam.ncol())
    stop("Dimensions of the hyperparameter matrix are inconsistent");
    
  if (hyperparam.nrow() != sizeMatr)
    stop("Hyperparameter and the transition matrices differ in dimensions");
    
  List _dimNames = hyperparam.attr("dimnames");

  if (_dimNames.size() == 0)
    stop("Provide dimnames for the hyperparameter matrix");
  
  CharacterVector _colNames = _dimNames[1];
  CharacterVector _rowNames = _dimNames[0];
  int sizeHyperparam = hyperparam.ncol();
  CharacterVector _sortedColNames(sizeHyperparam), _sortedRowNames(sizeHyperparam);
  
  for (int i = 0; i < sizeHyperparam; i++)
    _sortedColNames(i) = colNames(i), _sortedRowNames(i) = rowNames(i);
  
  _sortedColNames.sort();
  _sortedRowNames.sort();
  
  for (int i = 0; i < sizeHyperparam; i++)
    if (sortedColNames(i) != _sortedColNames(i) || sortedRowNames(i) != _sortedRowNames(i))
      stop("Hyperparameter and the transition matrices states differ");
  
  for (int i = 0; i < sizeMatr; i++)
    for (int j = 0; j < sizeMatr; j++)
      if (hyperparam(i, j) < 1.)
        stop("The hyperparameter elements must all be greater than or equal to 1");
 
  transMatr = sortByDimNames(transMatr);
  hyperparam = sortByDimNames(hyperparam);
  NumericVector logProbVec;
  
  for (int i = 0; i < sizeMatr; i++) {
    double logProb_i = 0., hyperparamRowSum = 0;
  
    for (int j = 0; j < sizeMatr; j++) {
      hyperparamRowSum += hyperparam(i, j);
      logProb_i += (hyperparam(i, j) - 1.) * log(transMatr(i, j)) - lgamma(hyperparam(i, j));
    }
    
    logProb_i += lgamma(hyperparamRowSum);
    logProbVec.push_back(logProb_i);
  }
  
  logProbVec.attr("names") = sortedColNames;

  return logProbVec;
}

// [[Rcpp::export(.hittingProbabilitiesRcpp)]]
NumericMatrix hittingProbabilities(S4 object) {
  NumericMatrix transitionMatrix = object.slot("transitionMatrix");
  CharacterVector states = object.slot("states");
  bool byrow = object.slot("byrow");
  
  if (!byrow)
    transitionMatrix = transpose(transitionMatrix);
  
  int numStates = transitionMatrix.nrow();
  arma::mat transitionProbs = as<arma::mat>(transitionMatrix);
  arma::mat hittingProbs(numStates, numStates);
  // Compute closed communicating classes
  List commClasses = commClassesKernel(transitionMatrix);
  List closedClass = commClasses["closed"];
  LogicalMatrix communicating = commClasses["classes"];

  
  for (int j = 0; j < numStates; ++j) {
    arma::mat coeffs = as<arma::mat>(transitionMatrix);
    arma::vec right_part = -transitionProbs.col(j);
    
    for (int i = 0; i < numStates; ++i) {
      coeffs(i, j) = 0;
      coeffs(i, i) -= 1;
    }

    for (int i = 0; i < numStates; ++i) {
      if (closedClass(i)) {
        for (int k = 0; k < numStates; ++k)
          if (k != i)
            coeffs(i, k) = 0;
          else
            coeffs(i, i) = 1;
          
        if (communicating(i, j))
          right_part(i) = 1;
        else
          right_part(i) = 0;
      }
    }
    
    hittingProbs.col(j) = arma::solve(coeffs, right_part);
  }
  
  NumericMatrix result = wrap(hittingProbs);
  colnames(result) = states;
  rownames(result) = states;
  
  if (!byrow)
    result = transpose(result);
  
  return result;
}



// method to convert into canonic form a markovchain object
// [[Rcpp::export(.canonicFormRcpp)]]
S4 canonicForm(S4 obj) {
  NumericMatrix transitions = obj.slot("transitionMatrix");
  bool byrow = obj.slot("byrow");
  int numRows = transitions.nrow();
  int numCols = transitions.ncol();
  NumericMatrix resultTransitions(numRows, numCols);
  CharacterVector states = obj.slot("states");
  unordered_map<string, int> stateToIndex;
  unordered_set<int> usedIndices;
  int currentIndex;
  List recClasses;
  S4 input("markovchain");
  S4 result("markovchain");
  vector<int> indexPermutation(numRows);
  
  if (!byrow) {
    input.slot("transitionMatrix") = transpose(transitions);
    input.slot("states") = states;
    input.slot("byrow") = true;
    transitions = transpose(transitions);
  } else {
    input = obj;
  }
  
  recClasses = recurrentClasses(input);
  
  // Map each state to the index it has
  for (int i = 0; i < states.size(); ++i) {
    string state = (string) states[i];
    stateToIndex[state] = i;
  }
  
  int toFill = 0;
  for (CharacterVector recClass : recClasses) {
    for (auto state : recClass) {
      currentIndex = stateToIndex[(string) state];
      indexPermutation[toFill] = currentIndex;
      ++toFill;
      usedIndices.insert(currentIndex);
    }
  }
  
  for (int i = 0; i < states.size(); ++i) {
    if (usedIndices.count(i) == 0) {
      indexPermutation[toFill] = i;
      ++toFill;
    }
  }
  
  CharacterVector newStates(numRows);
  
  for (int i = 0; i < numRows; ++i) {
    int r = indexPermutation[i];
    newStates(i) = states(r);
    
    for (int j = 0; j < numCols; ++j) {
      int c = indexPermutation[j];
      resultTransitions(i, j) = transitions(r, c);
    }
  }
  
  rownames(resultTransitions) = newStates;
  colnames(resultTransitions) = newStates;
  
  if (!byrow)
    resultTransitions = transpose(resultTransitions);
  
  result.slot("transitionMatrix") = resultTransitions;
  result.slot("byrow") = byrow;
  result.slot("states") = newStates;
  result.slot("name") = input.slot("name");
  return result;
}


// Function to sort a matrix of vectors lexicographically
NumericMatrix lexicographicalSort(NumericMatrix m) {
  int numCols = m.ncol();
  int numRows = m.nrow();
  
  if (numRows > 0 && numCols > 0) {
    vector<vector<double>> x(numRows, vector<double>(numCols));
    
    for (int i = 0; i < numRows; ++i)
      for (int j = 0; j < numCols; ++j)
        x[i][j] = m(i,j);
    
    sort(x.begin(), x.end());
    
    NumericMatrix result(numRows, numCols);
    
    for (int i = 0; i < numRows; ++i)
      for (int j = 0; j < numCols; ++j)
        result(i, j) = x[i][j];
    
    colnames(result) = colnames(m);
    return result;
  } else {
    return m;
  }
}

// Declared in utils.cpp
bool approxEqual(const cx_double& a, const cx_double& b);


mat computeSteadyStates(NumericMatrix t, bool byrow) {
  if (byrow)
    t = transpose(t);
  
  cx_mat transitionMatrix = as<cx_mat>(t);
  cx_vec eigvals;
  cx_mat eigvecs;
  // 1 + 0i
  cx_double cxOne(1.0, 0);
  bool correctEigenDecomposition;
  
  // If transition matrix is hermitian (symmetric in real case), use
  // more efficient implementation to get the eigenvalues and vectors
  if (transitionMatrix.is_hermitian()) {
    vec realEigvals;
    correctEigenDecomposition = eig_sym(realEigvals, eigvecs, transitionMatrix);
    eigvals.resize(realEigvals.size());
    
    // eigen values are real, but we need to cast them to complex values
    // to perform the rest of the algorithm
    for (int i = 0; i < realEigvals.size(); ++i)
      eigvals[i] = cx_double(realEigvals[i], 0);
  } else {
    correctEigenDecomposition = eig_gen(eigvals, eigvecs, transitionMatrix, "balance");
  }
  
  if (!correctEigenDecomposition)
    stop("Failure computing eigen values / vectors for submatrix in computeSteadySates");
  std::vector<int> whichOnes;
  std::vector<double> colSums;
  double colSum;
  mat realEigvecs = real(eigvecs);
  int numRows = realEigvecs.n_rows;
  
  // Search for the eigenvalues which are 1 and store 
  // the sum of the corresponding eigenvector
  for (int j = 0; j < eigvals.size(); ++j) {
    if (approxEqual(eigvals[j], cxOne)) {
      whichOnes.push_back(j);
      colSum = 0;
      
      for (int i = 0; i < numRows; ++i)
        colSum += realEigvecs(i, j);
      
      colSums.push_back((colSum != 0 ? colSum : 1));
    }
  }
  
  // Normalize eigen vectors
  int numCols = whichOnes.size();
  mat result(numRows, numCols);
  
  for (int j = 0; j < numCols; ++j)
    for (int i = 0; i < numRows; ++i)
      result(i, j) = realEigvecs(i, whichOnes[j]) / colSums[j];
  
  if (byrow)
    result = result.t();
  
  return result;
}


// Precondition: the matrix should be stochastic by rows
NumericMatrix steadyStatesByRecurrentClasses(S4 object) {
  List recClasses = recurrentClasses(object);
  int numRecClasses = recClasses.size();
  NumericMatrix transitionMatrix = object.slot("transitionMatrix");
  
  CharacterVector states = object.slot("states");
  int numCols = transitionMatrix.ncol();
  NumericMatrix steady(numRecClasses, numCols);
  unordered_map<string, int> stateToIndex;
  int steadyStateIndex = 0;
  bool negativeFound = false;
  double current;
  
  // Map each state to the index it has
  for (int i = 0; i < states.size(); ++i) {
    string state = (string) states[i];
    stateToIndex[state] = i;
  }
  
  // For each recurrent class, there must be an steady state
  for (CharacterVector recurrentClass : recClasses) {
    int recClassSize = recurrentClass.size();
    NumericMatrix subMatrix(recClassSize, recClassSize);
    
    // Fill the submatrix corresponding to the current steady class
    // Note that for that we have to subset the matrix with the indices
    // the states in the recurrent class ocuppied in the transition matrix
    for (int i = 0; i < recClassSize; ++i) {
      int r = stateToIndex[(string) recurrentClass[i]];
      
      for (int j = 0; j < recClassSize; ++j) {
        int c = stateToIndex[(string) recurrentClass[j]];
        subMatrix(i, j) = transitionMatrix(r, c);
      }
    }
    
    // Compute the steady states for the given submatrix
    mat steadySubMatrix = computeSteadyStates(subMatrix, true);
    
    // There should only be one steady state for that matrix
    // Make sure of it
    if (steadySubMatrix.n_rows != 1)
      stop("Could not compute steady states with recurrent classes method");
    
    for (int i = 0; i < recClassSize && !negativeFound; ++i) {
      int c = stateToIndex[(string) recurrentClass[i]];
      // Either all elements are positive or only a few are negative, because
      // computeSteadyStates normalizes the result dividing by the sum of the vector
      current = steadySubMatrix(0, i);
      // If we find some negative value from the steady states,  we should stop the method
      negativeFound = current < 0;
      steady(steadyStateIndex, c) = current;
    }
    
    if (negativeFound)
      stop("Could not compute steady states correctly: negative value found"); 
    
    ++steadyStateIndex;
  }
  
  colnames(steady) = states;
  
  return steady;
}

// [[Rcpp::export(.steadyStatesRcpp)]]
NumericMatrix steadyStates(S4 obj) {
  NumericMatrix transitions = obj.slot("transitionMatrix");
  CharacterVector states = obj.slot("states");
  bool byrow = obj.slot("byrow");
  S4 object("markovchain");
  
  if (!byrow) {
    object.slot("transitionMatrix") = transpose(transitions);
    object.slot("states") = states;
    object.slot("byrow") = true;
  } else {
    object = obj;
  }
  
  // Compute steady states using recurrent classes (there is one
  // steady state associated with each recurrent class)
  NumericMatrix result = lexicographicalSort(steadyStatesByRecurrentClasses(object));
  
  if (!byrow)
    result = transpose(result);
  
  return result;
}

