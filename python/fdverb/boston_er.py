import numpy as np

# Boston Symphony Hall early reflections
# source: James Moorer, "About This Reverberation Business", Table 3, p. 24
# generated using geometric simulation
num_taps = 18
times = np.array([.0043, .0215, .0225, .0268, .0270, .0298, .0458, .0485, .0572, .0587, .0595, .0612, .0707, .0708, .0726, .0741, .0753, .0797])
gains = np.array([.841,  .504,  .491,  .379,  .380,  .346,  .289,  .272,  .192,  .193,  .217,  .181,  .180,  .181,  .176,  .142,  .167,  .134])
