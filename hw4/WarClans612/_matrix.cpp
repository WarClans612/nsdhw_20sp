// Developer: Wilbert (wilbert.phen@gmail.com)

#include <iostream>
#include "mkl.h"

#include "_matrix.hpp"
#include "_block.hpp"
#include "_tiler.hpp"

// default contructor
Matrix::Matrix(size_t nrow, size_t ncol)
    : m_nrow(nrow), m_ncol(ncol), m_buffer(nrow * ncol, 0)
{
    std::fill(m_buffer.begin(), m_buffer.end(), 0);
}

// copy constructor
Matrix::Matrix(Matrix const & other)
    : m_nrow(other.m_nrow), m_ncol(other.m_ncol), m_buffer((other.m_nrow) * (other.m_ncol), 0)
{
    for(size_t i=0; i < m_nrow; ++i)
    {
        const size_t base_t = i*m_ncol;
        const size_t base_s = i*other.m_ncol;
        for (size_t j=0; j < m_ncol; ++j)
            if (i >= other.m_nrow || j >= other.m_ncol) m_buffer.at(base_t + j) = 0;
            else m_buffer.at(base_t + j) = other.m_buffer.at(base_s + j);
    }
}

// move constructor
Matrix::Matrix(Matrix && other)
    : m_nrow(other.m_nrow), m_ncol(other.m_ncol), m_buffer(other.m_nrow * other.m_ncol, 0)
{
    other.m_buffer.swap(m_buffer);
}

Matrix::Matrix(std::vector<std::vector<double>> const & other)
    : m_nrow(other.size()), m_ncol(other[0].size())
{
    for(const auto &v: other)
        m_buffer.insert(m_buffer.end(), v.begin(), v.end()); 
}

void validate_multiplication(const Matrix &mat1, const Matrix &mat2)
{
    if (mat1.m_ncol != mat2.m_nrow)
    {
        throw std::out_of_range(
            "the number of first matrix column "
            "differs from that of second matrix row");
    }
}

Matrix multiply_naive(const Matrix &mat1, const Matrix &mat2)
{
    validate_multiplication(mat1, mat2);

    // New matrix to be returned
    Matrix ret(mat1.nrow(), mat2.ncol());

    for (size_t i=0; i<ret.nrow(); ++i)
    {
        for (size_t k=0; k<ret.ncol(); ++k)
        {
            double v = 0;
            for (size_t j=0; j<mat1.ncol(); ++j)
            {
                v += mat1(i,j) * mat2(j,k);
            }
            ret(i, k) = v;
        }
    }
    return ret;
};

Matrix multiply_mkl(const Matrix &mat1, const Matrix &mat2)
{
    validate_multiplication(mat1, mat2);

    // New matrix to be returned
    Matrix ret(mat1.m_nrow, mat2.m_ncol);

    cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
        mat1.m_nrow, mat2.m_ncol, mat1.m_ncol, 1.0,
        mat1.m_buffer.data(), mat1.m_ncol,
        mat2.m_buffer.data(), mat2.m_ncol,
        0.0,
        ret.m_buffer.data(), mat2.m_ncol
    );
    return ret;
};

Matrix multiply_tile(const Matrix &mat1, const Matrix &mat2, const int tsize)
{
    validate_multiplication(mat1, mat2);

    // New matrix to be returned
    Matrix ret(mat1.m_nrow, mat2.m_ncol);

    for (size_t it=0, b_row=mat1.m_nrow%tsize; it<mat1.m_nrow; it+=b_row, b_row=tsize)
    {
        //Getting output row size of block
        //size_t r_row = mat1.m_nrow-it;
        //if ( r_row >= tsize) b_row = tsize;
        //else b_row = r_row;
        for (size_t kt=0, b_col=mat2.m_ncol%tsize; kt<mat2.m_ncol; kt+=b_col, b_col=tsize)
        {
            //Getting output column size of block
            //size_t r_col = mat2.m_ncol-kt;
            //if ( r_col >= tsize) b_col = tsize;
            //else b_col = r_col;
            Block value(b_row, b_col);

            for (size_t jt=0, b_mid=mat1.m_ncol%tsize; jt<mat1.m_ncol; jt+=b_mid, b_mid=tsize)
            {
                //Getting size for mid of block multiplication
                //size_t r_mid = mat1.m_ncol-jt;
                //if ( r_mid >= tsize) b_mid = tsize;
                //else b_mid = r_mid;

                Tiler tiler(b_row, b_mid, b_col);

                tiler.load(mat1, it, jt, mat2, jt, kt);
                tiler.multiply(value);
            }
            value.save(ret, it, kt);
        }
    }

    return ret;
}
