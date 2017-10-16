/*
 * particle_filter.cpp
 *
 *  Created on: Dec 12, 2016
 *      Author: Tiffany Huang
 *		
 *		Submission / Code Completion
 *		@Adithya Ranga
 */

#include <random>
#include <algorithm>
#include <iostream>
#include <numeric>
#include <math.h> 
#include <iostream>
#include <sstream>
#include <string>
#include <iterator>

#include "particle_filter.h"

using namespace std;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
	// Set the number of particles. Initialize all particles to first position (based on estimates of 
	//   x, y, theta and their uncertainties from GPS) and all weights to 1. 
	// Add random Gaussian noise to each particle.

	// number of particles - meets requirements..!!
	num_particles = 100;

	// random num gen
	default_random_engine gen;

	// normal distb - noise with mean & std to gps measurement..!!
	normal_distribution<double> Nx_init(x, std[0]);
	normal_distribution<double> Ny_init(y, std[1]);
	normal_distribution<double> Ntheta_init(theta, std[2]);

	// initialize all 100 particles randomly..!!
	for(int i=0; i<num_particles; ++i)
	{
		Particle particle;
		particle.id = i;
		particle.x = Nx_init(gen);
		particle.y = Ny_init(gen);
		particle.theta = Ntheta_init(gen);
		particle.weight = 1;

		particles.push_back(particle);
		weights.push_back(1);
	}

	is_initialized = true;

}

void ParticleFilter::prediction(double delta_t, double std_pos[], double velocity, double yaw_rate) {
	// Add measurements to each particle and add random Gaussian noise.
	// NOTE: When adding noise you may find std::normal_distribution and std::default_random_engine useful.
	//  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
	//  http://www.cplusplus.com/reference/random/default_random_engine/

	default_random_engine gen;

	// CTRV motion model --> condition check for zero yaw rate..!!

	for(int i=0; i<num_particles; ++i)
	{
		if(yaw_rate == 0.0)
		{
			particles[i].x += velocity*delta_t*cos(particles[i].theta);
			particles[i].y += velocity*delta_t*sin(particles[i].theta);
		}
		else
		{
			particles[i].x += velocity / yaw_rate * (sin(particles[i].theta + (yaw_rate*delta_t)) - sin(particles[i].theta));
			particles[i].y += velocity / yaw_rate * (cos(particles[i].theta) - cos(particles[i].theta + (yaw_rate*delta_t)));
			particles[i].theta += yaw_rate * delta_t;
		}

		normal_distribution<double> Nx(particles[i].x, std_pos[0]);
		normal_distribution<double> Ny(particles[i].y, std_pos[1]);
		normal_distribution<double> Ntheta(particles[i].theta, std_pos[2]);

		// add jitter to predictions..!!
		particles[i].x = Nx(gen);
		particles[i].y = Ny(gen);
		particles[i].theta = Ntheta(gen);
	}

}

void ParticleFilter::dataAssociation(std::vector<LandmarkObs> predicted, std::vector<LandmarkObs>& observations) {
	// Find the predicted measurement that is closest to each observed measurement and assign the 
	//   observed measurement to this particular landmark.
	// NOTE: this method will NOT be called by the grading code. But you will probably find it useful to 
	//   implement this method and use it as a helper during the updateWeights phase.

	for(int i=0; i<observations.size(); ++i){

		double min_dist = numeric_limits<double>::max();

		int map_id;

		for(int j=0; j< predicted.size(); ++j){

			double curr_dist = dist(observations[i].x, observations[i].y, predicted[j].x, predicted[j].y);

			if(curr_dist < min_dist){
				min_dist = curr_dist;
				map_id = predicted[j].id;
			}

		}

		// record the lamdmark id 
		observations[i].id = map_id;
	}

}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], 
		const std::vector<LandmarkObs> &observations, const Map &map_landmarks) {
	// Update the weights of each particle using a mult-variate Gaussian distribution. You can read
	//   more about this distribution here: https://en.wikipedia.org/wiki/Multivariate_normal_distribution
	// NOTE: The observations are given in the VEHICLE'S coordinate system. Your particles are located
	//   according to the MAP'S coordinate system. You will need to transform between the two systems.
	//   Keep in mind that this transformation requires both rotation AND translation (but no scaling).
	//   The following is a good resource for the theory:
	//   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
	//   and the following is a good resource for the actual equation to implement (look at equation 
	//   3.33
	//   http://planning.cs.uiuc.edu/node99.html

	for(int i=0; i < num_particles; ++i){

		vector<LandmarkObs> predictions;

		for(int k=0; k < map_landmarks.landmark_list.size(); ++k){

			double lamdmark_x = map_landmarks.landmark_list[k].x_f;
			double lamdmark_y = map_landmarks.landmark_list[k].y_f;
			int landmark_id = map_landmarks.landmark_list[k].id_i;

			if(fabs(particles[i].x - lamdmark_x) <= sensor_range && fabs(particles[i].y - lamdmark_y) <= sensor_range){
				predictions.push_back(LandmarkObs{landmark_id, lamdmark_x, lamdmark_y});
			}
		}

		vector<LandmarkObs> trans_obs;

		// Transform observations from car coordinates to map reference..!!
    	for (int j = 0; j < observations.size(); ++j) {

      		double trans_x = cos(particles[i].theta)*observations[j].x - sin(particles[i].theta)*observations[j].y + particles[i].x;
      		double trans_y = sin(particles[i].theta)*observations[j].x + cos(particles[i].theta)*observations[j].y + particles[i].y;

      		trans_obs.push_back(LandmarkObs{ observations[j].id, trans_x, trans_y });
      	}

      	// Run data association..!!
      	dataAssociation(predictions, trans_obs);

      	particles[i].weight = 1.0;

      	for(int k=0; k<trans_obs.size(); ++k){

      		double mu_x, mu_y;

      		for(int j=0; j<predictions.size(); ++j){

      			if(predictions[j].id == trans_obs[k].id){

      				mu_x = predictions[j].x;
      				mu_y = predictions[j].y;
      			}
      		}

      		double o_x = trans_obs[k].x;
      		double o_y = trans_obs[k].y;

      		double sx = std_landmark[0];
      		double sy = std_landmark[1];

      		// Multi variate gaussian...!!
      		double mvg = ( 1/(2*M_PI*sx*sy)) * exp( -( pow(mu_x-o_x,2)/(2*pow(sx, 2)) + (pow(mu_y-o_y,2)/(2*pow(sy, 2))) ) );

      		//Particle Weights..!!
			particles[i].weight *= mvg;

			weights[i] = particles[i].weight;

      	}
	}

}

void ParticleFilter::resample() {
	// Resample particles with replacement with probability proportional to their weight. 
	// NOTE: You may find std::discrete_distribution helpful here.
	//   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution

	vector<Particle> resampled_particles (num_particles);
  
  	default_random_engine gen;

  	for (int i = 0; i < num_particles; ++i) {

    	discrete_distribution<int> index(weights.begin(), weights.end());
    	resampled_particles[i] = particles[index(gen)];
    
  	}
  
  	// Replace old particles with the resampled particles
	particles = resampled_particles;

}

Particle ParticleFilter::SetAssociations(Particle particle, std::vector<int> associations, std::vector<double> sense_x, std::vector<double> sense_y)
{
	//particle: the particle to assign each listed association, and association's (x,y) world coordinates mapping to
	// associations: The landmark id that goes along with each listed association
	// sense_x: the associations x mapping already converted to world coordinates
	// sense_y: the associations y mapping already converted to world coordinates

	//Clear the previous associations
	particle.associations.clear();
	particle.sense_x.clear();
	particle.sense_y.clear();

	particle.associations= associations;
 	particle.sense_x = sense_x;
 	particle.sense_y = sense_y;

 	return particle;
}

string ParticleFilter::getAssociations(Particle best)
{
	vector<int> v = best.associations;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<int>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseX(Particle best)
{
	vector<double> v = best.sense_x;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
string ParticleFilter::getSenseY(Particle best)
{
	vector<double> v = best.sense_y;
	stringstream ss;
    copy( v.begin(), v.end(), ostream_iterator<float>(ss, " "));
    string s = ss.str();
    s = s.substr(0, s.length()-1);  // get rid of the trailing space
    return s;
}
