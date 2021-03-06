% Generated by roxygen2: do not edit by hand
% Please edit documentation in R/fittingFunctions.R
\name{markovchainSequence}
\alias{markovchainSequence}
\title{Function to generate a sequence of states from homogeneous Markov chains.}
\usage{
markovchainSequence(n, markovchain, t0 = sample(markovchain@states, 1),
  include.t0 = FALSE, useRCpp = TRUE)
}
\arguments{
\item{n}{Sample size}

\item{markovchain}{\code{markovchain} object}

\item{t0}{The initial state}

\item{include.t0}{Specify if the initial state shall be used}

\item{useRCpp}{Boolean. Should RCpp fast implementation being used? Default is yes.}
}
\value{
A Character Vector
}
\description{
Provided any \code{markovchain} object, it returns a sequence of 
states coming from the underlying stationary distribution.
}
\details{
A sequence of size n is sampled.
}
\examples{
# define the markovchain object
statesNames <- c("a", "b", "c")
mcB <- new("markovchain", states = statesNames, 
   transitionMatrix = matrix(c(0.2, 0.5, 0.3, 0, 0.2, 0.8, 0.1, 0.8, 0.1), 
   nrow = 3, byrow = TRUE, dimnames = list(statesNames, statesNames)))

# show the sequence
outs <- markovchainSequence(n = 100, markovchain = mcB, t0 = "a")

}
\references{
A First Course in Probability (8th Edition), Sheldon Ross, Prentice Hall 2010
}
\seealso{
\code{\link{markovchainFit}}
}
\author{
Giorgio Spedicato
}
