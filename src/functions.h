// This is a header file containing functions

#ifndef _FUNCTIONS_H
#define _FUNCTIONS_H

#include "utility.h"
#include "NanoParticle.h"
#include "particle.h"
#include "vertex.h"
#include "control.h"
#include "BinShell.h"
#include "BinRing.h"
#include "thermostat.h"
#include "forces.h"
#include "energies.h"
#include "mpi_utility.h"


#define PBSTR "||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||"
#define PBWIDTH 60
// general functions
// -----------------

// overloaded << to print 3d vectors
ostream &operator<<(ostream &, VECTOR3D);

// initialize particle velocities
void initialize_particle_velocities(vector<PARTICLE> &, vector<THERMOSTAT> &, NanoParticle *);

// initialize fake velocities
void initialize_fake_velocities(vector<VERTEX> &, vector<THERMOSTAT> &, NanoParticle *);

// fictious molecular dynamics (fmd)
void fmd(vector<VERTEX> &, vector<PARTICLE> &, NanoParticle *, CONTROL &, CONTROL &);

// car parrinello molecular dynamics (cpmd)
void cpmd(vector<PARTICLE> &, vector<VERTEX> &, NanoParticle *, vector<THERMOSTAT> &, vector<THERMOSTAT> &, CONTROL &, CONTROL &);

// compute and write useful data in cpmd
void compute_n_write_useful_data(int, vector<PARTICLE> &, vector<VERTEX> &, vector<THERMOSTAT> &, vector<THERMOSTAT> &,
                                 NanoParticle *);

// verify with F M D
double verify_with_FMD(int, vector<VERTEX>, vector<PARTICLE> &, NanoParticle *, CONTROL &, CONTROL &);

// make movie
void make_movie(int num, vector<PARTICLE> &, NanoParticle *);


// post analysis : compute R
double compute_MD_trust_factor_R(int);

// post analysis : compute T factor
double compute_MD_trust_factor_R_v(int);

// display progress bar (code from the internet)
void progressBar(double);

// post analysis : auto correlation function
//void auto_correlation_function();


// functions useful in computing forces and energies
// -------------------------------------------------

// Green's function G
inline long double G(vector<VERTEX> &s, unsigned int k, unsigned int l) {
    if (l != k) return 1.0 / (s[k].posvec - s[l].posvec).GetMagnitude();
    if (l == k) return 2 * sqrt(pi) * sqrt(s[k].a) / s[k].a;
    return 0.0;
}

// computes gradient of green's fn
inline VECTOR3D Grad(VECTOR3D &vec1, VECTOR3D &vec2) {
    long double r = (vec1 - vec2).GetMagnitude();
    long double r3 = r * r * r;
    return ((vec1 - vec2) ^ ((-1.0) / r3));
}

// Gradient of Green's function denoted by H
inline long double H(vector<VERTEX> &s, unsigned int k, unsigned int l, double radius) {
    if (l != k) return s[l].normalvec * Grad(s[l].posvec, s[k].posvec);
    if (l == k) return -0.5 * sqrt(pi) * sqrt(s[k].a) / (radius * s[k].a);
    return 0.0;
}

// computes gradient of normal dot gradient of 1/r
inline VECTOR3D GradndotGrad(VECTOR3D &vec1, VECTOR3D &vec2, VECTOR3D &normal) {
    long double r = (vec1 - vec2).GetMagnitude();
    long double r3 = r * r * r;
    long double r5 = r3 * r * r;
    return ((normal ^ (1.0 / r3)) - ((vec1 - vec2) ^ (3 * (normal * (vec1 - vec2)) / r5)));
}

// functions useful in implementing constraint
// -------------------------------------------

// constraint equation
inline long double constraint(vector<VERTEX> &s, vector<PARTICLE> &ion, NanoParticle *nanoParticle) {
    return (nanoParticle->total_induced_charge(s) -
            nanoParticle->total_charge_inside(ion) * (1 / nanoParticle->eout - 1 / nanoParticle->ein));
}

// SHAKE to ensure constraint is true
inline void SHAKE(vector<VERTEX> &s, vector<PARTICLE> &ion, NanoParticle *nanoParticle,
                  CONTROL &simremote)    // remote of the considered simulation
{
    long double sigma = constraint(s, ion, nanoParticle);
    for (unsigned int k = 0; k < s.size(); k++)
        s[k].vw = s[k].vw - (1.0 / simremote.timestep) * sigma / (s[k].a * int(s.size()));
    for (unsigned int k = 0; k < s.size(); k++)
        s[k].w = s[k].w - sigma / (s[k].a * int(s.size()));
    return;
}

// dot constraint equation
inline long double dotconstraint(vector<VERTEX> &s) {
    long double sigmadot = 0;
    for (unsigned int k = 0; k < s.size(); k++)
        sigmadot += s[k].vw * s[k].a;
    return sigmadot;
}

// RATTLE to ensure time derivative of the constraint is true
inline void RATTLE(vector<VERTEX> &s) {
    long double sigmadot = dotconstraint(s);
    for (unsigned int k = 0; k < s.size(); k++)
        s[k].vw = s[k].vw - sigmadot / (s[k].a * int(s.size()));
    return;
}

// functions useful in Nose-Hoover chain implementation
// -------------------------------------------

// update bath xi value
inline void update_chain_xi(unsigned int j, vector<THERMOSTAT> &bath, double dt, long double ke) {
    if (bath[j].Q == 0)
        return;
    if (j != 0)
        bath[j].xi = bath[j].xi * exp(-0.5 * dt * bath[j + 1].xi) + 0.5 * dt * (1.0 / bath[j].Q) *
                                                                    (bath[j - 1].Q * bath[j - 1].xi * bath[j - 1].xi -
                                                                     bath[j].dof * kB * bath[j].T) *
                                                                    exp(-0.25 * dt * bath[j + 1].xi);
//     bath[j].xi = bath[j].xi * exp(-0.5 * dt * bath[j+1].xi) + 0.5 * dt * (1.0 / bath[j].Q) * (bath[j-1].Q * bath[j-1].xi * bath[j-1].xi - bath[j].dof * kB * bath[j].T) * (exp(-0.5 * dt * bath[j+1].xi) - 1) / (-0.5 * dt * bath[j+1].xi);
    else
        bath[j].xi = ((bath[j].xi * exp(-0.5 * dt * bath[j + 1].xi)) +
                (0.5 * dt * (1.0 / bath[j].Q) * (2 * ke - bath[j].dof * kB * bath[j].T) *
                     exp(-0.25 * dt * bath[j + 1].xi)));
//     bath[j].xi = bath[j].xi * exp(-0.5 * dt * bath[j+1].xi) + 0.5 * dt * (1.0 / bath[j].Q) * (2*ke - bath[j].dof * kB * bath[j].T) * (exp(-0.5 * dt * bath[j+1].xi) - 1) / (-0.5 * dt * bath[j+1].xi);
    return;
}

// functions that write data files
// -------------------------------

inline void
write_basic_files(int write, int cpmdstep, vector<PARTICLE> &ion, vector<VERTEX> &s, vector<THERMOSTAT> &real_bath,
                  vector<THERMOSTAT> &fake_bath, NanoParticle *nanoParticle) {
    
    

    if (cpmdstep % write != 0)
        return;

    if (world.rank() == 0) {
        ofstream list_position("outfiles/ion_position.dat", ios::app);
        ofstream list_velocity("outfiles/ion_velocity.dat", ios::app);
        ofstream list_force("outfiles/ion_force.dat", ios::app);
        ofstream list_fake("outfiles/fake_values.dat", ios::app);
        ofstream list_bath("outfiles/bath_values.dat", ios::app);

        list_position << cpmdstep << setw(15) << ion[0].posvec.GetMagnitude() << setw(15)
                      << ion[1].posvec.GetMagnitude()
                      << setw(15) << ion[2].posvec.GetMagnitude() << endl;
        list_velocity << cpmdstep << setw(15) << ion[0].velvec.GetMagnitude() << setw(15)
                      << ion[1].velvec.GetMagnitude()
                      << setw(15) << ion[2].velvec.GetMagnitude() << endl;
        list_force << cpmdstep << setw(15) << ion[0].forvec.GetMagnitude() << setw(15) << ion[1].forvec.GetMagnitude()
                   << setw(15) << ion[2].forvec.GetMagnitude() << endl;
        list_fake << cpmdstep << setw(15) << s[0].w << setw(15) << s[0].vw << setw(15) << s[0].fw << endl;
        list_bath << cpmdstep << setw(15) << real_bath[0].xi << setw(15) << real_bath[0].eta << setw(15)
                  << fake_bath[0].xi
                  << setw(15) << fake_bath[0].eta << endl;
    }
    return;
}

#endif
