// This is gel/mrc/vpgl/algo/vpgl_em_compute_5_point.h
#ifndef vpgl_em_compute_5_point_h_
#define vpgl_em_compute_5_point_h_
//:
// \file
// \brief The 5 point algorithm as described by David Nister for computing an essential matrix from point correspondences.
// \author Noah Snavely, ported to VXL by Andrew Hoelscher
// \date April 24, 2011

#include <vcl_vector.h>
#include <vcl_iostream.h>
#include <vcl_cassert.h>

#include <vnl/vnl_matrix.h>
#include <vnl/vnl_vector_fixed.h>
#include <vnl/vnl_real_npolynomial.h>
#include <vnl/vnl_rank.h>
#include <vnl/algo/vnl_real_eigensystem.h>
#include <vnl/algo/vnl_svd.h>

#include <vgl/vgl_point_2d.h>

#include <vpgl/vpgl_essential_matrix.h>


template <class T>
class vpgl_em_compute_5_point
{
  public:

    vpgl_em_compute_5_point(): verbose(false), tolerance(0.0001) { }
    vpgl_em_compute_5_point(bool v, double t): verbose(v), tolerance(t) { }

    //: Compute from two sets of corresponding points.
    // Puts the resulting matrix into em, returns true if successful.
    // Each of right_points and left_points must contain exactly 5 points!
    // This function returns a set of potential solutions, generally 10.
    // Each of these solutions is appropriate to use as RANSAC hypthosis,
    // and generally done. The points must be normalized.
    bool compute( const vcl_vector<vgl_point_2d<T> > &right_points,
                  const vcl_vector<vgl_point_2d<T> > &left_points,
                  vcl_vector<vpgl_essential_matrix<T> > &ems);

  protected:
    bool verbose;
    double tolerance;

    void compute_nullspace_basis(
        const vcl_vector<vgl_point_2d<T> > &right_points,
        const vcl_vector<vgl_point_2d<T> > &left_points,
        vcl_vector<vnl_vector_fixed<T, 9> > &basis);

    void compute_constraint_polynomials(
        const vcl_vector<vnl_vector_fixed<T,9> > &basis,
        vcl_vector<vnl_real_npolynomial> &constraints);

    void compute_groebner_basis(
        const vcl_vector<vnl_real_npolynomial> &constraints,
        vnl_matrix<double> &groebner_basis);

    void compute_action_matrix(
        const vnl_matrix<double> &groebner_basis,
        vnl_matrix<double> &action_matrix);

    void compute_e_matrices(
        const vcl_vector<vnl_vector_fixed<T, 9> > &basis,
        const vnl_matrix<double> &action_matrix,
        vcl_vector<vpgl_essential_matrix<T> > &ems);

    double get_coeff(const vnl_real_npolynomial &p,
        int x_p, int y_p, int z_p);
};


/*-----------------------------------------------------------------------*/
template <class T>
bool vpgl_em_compute_5_point<T>::compute(
    const vcl_vector<vgl_point_2d<T> > &right_points,
    const vcl_vector<vgl_point_2d<T> > &left_points,
    vcl_vector<vpgl_essential_matrix<T> > &ems)
{
    // Check that we have the right number of points
    if (right_points.size() != 5 || left_points.size() != 5){
        if (verbose){
            vcl_cerr<<"Wrong number of input points!\n" <<
                "right_points has "<<right_points.size() <<
                " and left_points has "<<left_points.size() << '\n';
        }

        return false;
    }


    // Compute the null space basis of the epipolar constraint matrix
    vcl_vector<vnl_vector_fixed<T,9> > basis;
    compute_nullspace_basis(right_points, left_points, basis);

    // Using this basis, we now can compute the polynomial constraints
    // on the E matrix.
    vcl_vector<vnl_real_npolynomial> constraints;
    compute_constraint_polynomials(basis, constraints);

    // Find the groebner basis
    vnl_matrix<double> groebner_basis(10, 10);
    compute_groebner_basis(constraints, groebner_basis);

    // Action matrix
    vnl_matrix<double> action_matrix(10, 10);
    compute_action_matrix(groebner_basis, action_matrix);

    // Finally, use the action matrix to compute the essential matrices,
    // one possibility for each real eigenvalue of the action matrix
    compute_e_matrices(basis, action_matrix, ems);
}


//:
// Constructs the 5x9 epipolar constraint matrix based off the constraint
// that q1' * E * q2 = 0, and fills the vectors x, y, z and
// w with the nullspace basis for this matrix
template <class T>
void vpgl_em_compute_5_point<T>::compute_nullspace_basis(
    const vcl_vector<vgl_point_2d<T> > &right_points,
    const vcl_vector<vgl_point_2d<T> > &left_points,
    vcl_vector<vnl_vector_fixed<T, 9> > &basis)
{
    // Create the 5x9 epipolar constraint matrix
    vnl_matrix<T> A(5, 9);

    for (int i = 0; i < 5; i++) {
        A.put(i, 0, right_points[i].x() * left_points[i].x());
        A.put(i, 1, right_points[i].y() * left_points[i].x());
        A.put(i, 2, left_points[i].x());

        A.put(i, 3, right_points[i].x() * left_points[i].y());
        A.put(i, 4, right_points[i].y() * left_points[i].y());
        A.put(i, 5, left_points[i].y());

        A.put(i, 6, right_points[i].x());
        A.put(i, 7, right_points[i].y());
        A.put(i, 8, 1.0);
    }


    // Find four vectors that span the right nullspace of the matrix.
    // Do this using SVD.
    vnl_svd<T> svd(A, tolerance);
    vnl_matrix<T> V = svd.V();

    // The null space is spanned by the last four columns of V.
    for (int i = 5; i < 9; i++){
        vnl_vector_fixed<T,9> basis_vector;

        for (int j = 0; j < 9; j++){
            basis_vector[j] = V.get(j, i);
        }

        basis.push_back(basis_vector);
    }
}

//:
// Finds 10 constraint polynomials, based on the following criteria:
// if X, Y, Z and W are the basis vectors, then
// E = xX + yY + zZ + wW for some scalars x, y, z, w. Since these are
// unique up to a scale, we say w = 1;
//
// Furthermore, det(E) = 0, and E*E'*E - .5 * trace(E*E') * E = 0.
// Substituting the original equation into all 10 of the equations
// generated by these two constraints gives us the constraint polynomials.
template <class T>
void vpgl_em_compute_5_point<T>::compute_constraint_polynomials(
    const vcl_vector<vnl_vector_fixed<T,9> > &basis,
    vcl_vector<vnl_real_npolynomial> &constraints)
{
    // Create a polynomial for each entry of E.
    //
    // E = [e11 e12 e13] = x * [ X11 ... ...] + ...
    //     [e21 e22 e23]       [...  ... ...]
    //     [e31 e32 e33]       [...  ... ...]
    //
    // This means e11 = x * X11 + y * Y11 + z * Z11 + W11.
    // Form these polynomials. They will be used in the other constraints.
    vcl_vector<vnl_real_npolynomial> entry_polynomials(9);
    vnl_vector<double> coeffs(4);

    vnl_matrix<unsigned> exps(4, 4);
    exps.set_identity();
    exps.put(3, 3, 0);

    for (int i = 0; i < 9; i++) {
        coeffs[0] = basis[0][i];
        coeffs[1] = basis[1][i];
        coeffs[2] = basis[2][i];
        coeffs[3] = basis[3][i];

        entry_polynomials[i].set(coeffs, exps);
    }

    // Now we are going to create a polynomial from the constraint det(E)= 0.
    // if E = [a b c; d e f; g h i], (E = [0 1 2; 3 4 5; 6 7 8]) then
    // det(E) = (ai - gc) * e +  (bg - ah) * f + (ch - bi) * d.
    // We have a through i in terms of the basis vectors from above, so
    // use those to construct a constraint polynomial, and put it into
    // constraints.

    // e * (ai-gc) = 4 * (0*8 - 6*2)
    vnl_real_npolynomial det_term_1 = entry_polynomials[4] *
        (entry_polynomials[0] * entry_polynomials[8] -
         entry_polynomials[6] * entry_polynomials[2]);

    // f * (bg - ah) = 5 * (1*6 - 0*7)
    vnl_real_npolynomial det_term_2 = entry_polynomials[5] *
        (entry_polynomials[1] * entry_polynomials[6] -
         entry_polynomials[0] * entry_polynomials[7]);

    // d * (ch - bi) = 3 * (2*7 - 1*8)
    vnl_real_npolynomial det_term_3 = entry_polynomials[3] *
        (entry_polynomials[2] * entry_polynomials[7] -
         entry_polynomials[1] * entry_polynomials[8]);

    constraints.push_back(det_term_1 + det_term_2 + det_term_3);

    // Create polynomials for the singular value constraint.
    // These nine equations are from the constraint
    // E*E'*E - .5 * trace(E*E') * E = 0. If you want to see these in
    // their full glory, type the following snippet into matlab
    // (not octave, won't work).
    //
    // syms a b c d e f g h i;
    // E = [a b c; d e f; g h i];
    // pretty(2*E*E'*E - trace(E*E')*E)
    //
    // The first polynomial is this:
    //  a(2*a*a+ 2*b*b+ 2*c*c)+ d(2*a*d+ 2*b*e+ 2*c*f)+ g(2*a*g+ 2*b*h+ 2*c*i)
    //  - a(a*a+b*b+c*c+d*d+e*e+f*f+g*g+h*h+i*i)
    // The other polynomials have similar forms.

     // Define a*a + b*b + c*c + d*d + e*e + f*f + g*g + h*h + i*i, a
     // term in all other constraint polynomials
     vnl_real_npolynomial sum_of_squares =
        entry_polynomials[0] * entry_polynomials[0];

     for (int i = 1; i < 9; i++){
        sum_of_squares = sum_of_squares +
            entry_polynomials[i] * entry_polynomials[i];
     }

    // Create the first two terms in each polynomial and add it to
    // constraints
    for (int i = 0; i < 9; i++){
        constraints.push_back(
            entry_polynomials[i%3] *
                (entry_polynomials[0] * entry_polynomials[3*(i%3) + 0]*2 +
                 entry_polynomials[1] * entry_polynomials[3*(i%3) + 1]*2 +
                 entry_polynomials[2] * entry_polynomials[3*(i%3) + 2]*2)

            - entry_polynomials[i] * sum_of_squares);
    }

    // Now add the next term (there are four terms total)
    for (int i = 0; i < 9; i++){
        constraints[i] = constraints[i] +
            entry_polynomials[(i%3) + 3] *
                (entry_polynomials[3] * entry_polynomials[3*(i%3) + 0]*2 +
                 entry_polynomials[4] * entry_polynomials[3*(i%3) + 1]*2 +
                 entry_polynomials[5] * entry_polynomials[3*(i%3) + 2]*2);
    }

    // Last term
    for (int i = 0; i < 9; i++){
        constraints[i] = constraints[i] +
            entry_polynomials[(i%3) + 6] *
                (entry_polynomials[6] * entry_polynomials[3*(i%3) + 0]*2 +
                 entry_polynomials[7] * entry_polynomials[3*(i%3) + 1]*2 +
                 entry_polynomials[8] * entry_polynomials[3*(i%3) + 2]*2);
    }
}


/*-----------------------------------------------------------------------*/
//:
// Returns the coefficient of a term of a vnl_real_npolynomial in three
// variables with an x power of x_p, a y power of y_p and a z power of z_p
template <class T>
double vpgl_em_compute_5_point<T>::get_coeff(
    const vnl_real_npolynomial &p, int x_p, int y_p, int z_p)
{
    for (int i = 0; i < p.polyn().rows(); i++){
        if (x_p == p.polyn().get(i, 0) and
           y_p == p.polyn().get(i, 1) and
           z_p == p.polyn().get(i, 2)){
            return p.coefficients()[i];
        }
    }

    return -1;
}

template <class T>
void vpgl_em_compute_5_point<T>::compute_groebner_basis(
    const vcl_vector<vnl_real_npolynomial> &constraints,
    vnl_matrix<double> &groebner_basis)
{
    assert(groebner_basis.rows() == 10);
    assert(groebner_basis.cols() == 10);

    vnl_matrix<double> A(10, 20);

    for (int i = 0; i < 10; i++) {
        // x3 x2y xy2 y3 x2z xyz y2z xz2 yz2 z3 x2 xy y2 xz yz z2 x  y  z  1
        A.put(i, 0, get_coeff(constraints[i], 3, 0, 0));
        A.put(i, 1, get_coeff(constraints[i], 2, 1, 0));
        A.put(i, 2, get_coeff(constraints[i], 1, 2, 0));
        A.put(i, 3, get_coeff(constraints[i], 0, 3, 0));
        A.put(i, 4, get_coeff(constraints[i], 2, 0, 1));
        A.put(i, 5, get_coeff(constraints[i], 1, 1, 1));
        A.put(i, 6, get_coeff(constraints[i], 0, 2, 1));
        A.put(i, 7, get_coeff(constraints[i], 1, 0, 2));
        A.put(i, 8, get_coeff(constraints[i], 0, 1, 2));
        A.put(i, 9, get_coeff(constraints[i], 0, 0, 3));
        A.put(i, 10, get_coeff(constraints[i], 2, 0, 0));
        A.put(i, 11, get_coeff(constraints[i], 1, 1, 0));
        A.put(i, 12, get_coeff(constraints[i], 0, 2, 0));
        A.put(i, 13, get_coeff(constraints[i], 1, 0, 1));
        A.put(i, 14, get_coeff(constraints[i], 0, 1, 1));
        A.put(i, 15, get_coeff(constraints[i], 0, 0, 2));
        A.put(i, 16, get_coeff(constraints[i], 1, 0, 0));
        A.put(i, 17, get_coeff(constraints[i], 0, 1, 0));
        A.put(i, 18, get_coeff(constraints[i], 0, 0, 1));
        A.put(i, 19, get_coeff(constraints[i], 0, 0, 0));
    }

    // Do a full Gaussian elimination
    vnl_matrix<double> rrefed = vnl_rank_row_reduce<double>(A);

    // Copy out results. Since the first 10*10 block of A is the
    // identity (due to the row_reduce), we are interested in the
    // second 10*10 block.
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++){
            groebner_basis.put(i, j, A.get(i, j+10));
        }
    }
}


/*-----------------------------------------------------------------------*/
template <class T>
void vpgl_em_compute_5_point<T>::compute_action_matrix(
    const vnl_matrix<double> &groebner_basis,
    vnl_matrix<double> &action_matrix)
{
    action_matrix.fill(0.0);

    action_matrix.set_row(0, groebner_basis.get_row(0));
    action_matrix.set_row(1, groebner_basis.get_row(1));
    action_matrix.set_row(2, groebner_basis.get_row(2));
    action_matrix.set_row(3, groebner_basis.get_row(4));
    action_matrix.set_row(4, groebner_basis.get_row(5));
    action_matrix.set_row(5, groebner_basis.get_row(7));
    action_matrix *= -1;

    action_matrix.put(6, 0, 1.0);
    action_matrix.put(7, 1, 1.0);
    action_matrix.put(8, 3, 1.0);
    action_matrix.put(9, 6, 1.0);
}

/*------------------------------------------------------------------------*/
template <class T>
void vpgl_em_compute_5_point<T>::compute_e_matrices(
    const vcl_vector<vnl_vector_fixed<T, 9> > &basis,
    const vnl_matrix<double> &action_matrix,
    vcl_vector<vpgl_essential_matrix<T> > &ems)
{
    vnl_real_eigensystem eigs(action_matrix);

    for (int i = 0; i < eigs.D.size(); i++) {
        if (vcl_abs(eigs.D.get(i, i).imag()) <= tolerance){
            vnl_vector_fixed<T, 9> linear_e;

            double w_inv = 1.0 / eigs.V.get(i, 9).real();
            double x = eigs.V.get(i, 6).real() * w_inv;
            double y = eigs.V.get(i, 7).real() * w_inv;
            double z = eigs.V.get(i, 8).real() * w_inv;

            linear_e =
                x * basis[0] + y * basis[1] + z * basis[2] + basis[3];
            linear_e /= linear_e[8];

            ems.push_back(vpgl_essential_matrix<T>(
                vnl_matrix_fixed<T, 3, 3>(linear_e.data_block())));
        }
    }
}

#endif // vpgl_em_compute_5_point_h_