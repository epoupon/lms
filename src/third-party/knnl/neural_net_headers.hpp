/*
 * Copyright (c) 2006, Seweryn Habdank-Wojewodzki
 * Copyright (c) 2006, Janusz Rybarski
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms,
 * with or without modification, are permitted provided
 * that the following conditions are met:
 *
 * Redistributions of source code must retain the above
 * copyright notice, this list of conditions and the
 * following disclaimer.
 *
 * Redistributions in binary form must reproduce the
 * above copyright notice, this list of conditions
 * and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS
 * AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * e-mail: habdank AT gmail DOT com
 * e-mail: janusz.rybarski AT gmail DOT com
 *
 * File created: Fri 12 May 2006 14:39:01 CEST
 * Last modified: Sat 16 Dec 2006 16:06:28 CET
 */

#ifndef NEURAL_NET_HEADERS_HPP_INCLUDED
#define NEURAL_NET_HEADERS_HPP_INCLUDED

/** \mainpage Kohonen Neural Network Library
 *
 * \section intro_sec Introduction
 *
 * KNNL is header based library for developing Kohonen Neural Networks.
 *
 */


/**
 * \file neural_net_headers.hpp
 * \brief All headers for kohonen neural network.
 * \ingroup neural_net
 */

#include "basic_neuron.hpp"
#include "euclidean_distance_function.hpp"
#include "functors.hpp"
#include "generalized_training_weight.hpp"
#include "kohonen_network.hpp"
#include "print_network.hpp"
#include "randomize_policy.hpp"
#include "ranges.hpp"
#include "rectangular_container.hpp"
#include "training_functional.hpp"
#include "weighted_euclidean_distance_function.hpp"
#include "wta_training_algorithm.hpp"
#include "wtm_topology.hpp"
#include "wtm_training_algorithm.hpp"


#endif // NEURAL_NET_HEADERS_HPP_INCLUDED

