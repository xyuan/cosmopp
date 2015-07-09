#ifndef COSMO_PDE_HPP
#define COSMO_PDE_HPP

#include <vector>

#include <function.hpp>

namespace Math
{

class InitialValPDEInterface
{
public:
    InitialValPDEInterface() {}
    virtual ~InitialValPDEInterface() {}

    virtual int spaceDim() const = 0;
    virtual int funcDim() const = 0;

    virtual void evaluate(double t, const std::vector<double>& x, const std::vector<double>& u, std::vector<std::vector<double> > *f, std::vector<double> *s) const = 0;
};

class InitialValPDESolver
{
public:
    InitialValPDESolver(InitialValPDEInterface *pde);
    ~InitialValPDESolver();

    void set(const std::vector<RealFunctionMultiDim*>& w0, const std::vector<double>& xMin, const std::vector<double>& xMax, const std::vector<int>& nx);

    void propagate(double t);

    void takeStep();

    double getCurrentT() const { return t_; }
    double getDeltaT() const { return deltaT_; }
    void setDeltaT(double deltaT) { check(deltaT > 0, ""); deltaT_ = deltaT; }

    int getNx(int i) const { check(i >= 0 && i < d_, ""); return nx_[i]; }
    int getNx0Starting() const { return nx0Starting_; }
    int getXMax(int i) const { check(i >= 0 && i < d_, ""); return xMax_[i]; }
    int getDeltaX(int i) const { check(i >= 0 && i < d_, ""); return deltaX_[i]; }

    const std::vector<double>& getField(const std::vector<int>& ind) const { return grid_[index(ind)]; }

    inline void physicalCoords(const std::vector<int>& i, std::vector<double> *coords) const;

private:
    void setupGrid();
    void setInitial(const std::vector<RealFunctionMultiDim*>& w0);
    void setOwnBoundary();

    void communicateBoundary();
    void sendLeft();
    void sendRight();
    void receiveLeft();
    void receiveRight();

    void saveBuffer(unsigned long start);
    void retrieveBuffer(unsigned long start);

    inline unsigned long index(const std::vector<int>& i) const;
    inline unsigned long halfIndex(const std::vector<int>& i) const;
    inline void increaseIndex(std::vector<int>& i, const std::vector<int>& rangeBegin, const std::vector<int>& rangeEnd) const;
    inline void physicalCoordsHalf(const std::vector<int>& i, std::vector<double> *coords) const;

private:
    const InitialValPDEInterface *pde_;

    const int d_;
    const int m_;

    double twoD_;

    int nProcesses_;
    int processId_;

    std::vector<double> xMin_;
    std::vector<double> xMax_;
    std::vector<double> deltaX_;
    std::vector<int> nx_;

    int nx0Starting_;

    std::vector<unsigned long> dimProd_;
    std::vector<unsigned long> halfDimProd_;

    std::vector<std::vector<double> > grid_;
    std::vector<std::vector<double> > halfGrid_;

    double t_;
    double deltaT_;

    int commTag_;
    std::vector<double> buffer_;
};

unsigned long
InitialValPDESolver::index(const std::vector<int>& i) const
{
    check(i.size() == d_, "");
    check(dimProd_.size() == d_ + 1, "");

    unsigned long res = 0;

    for(int j = 0; j < d_; ++j)
    {
        const int k = i[j];
        check(k >= -1 && k <= nx_[j], "");

        res += (unsigned long)(k + 1) * dimProd_[j + 1];
    }

    return res;
}

unsigned long
InitialValPDESolver::halfIndex(const std::vector<int>& i) const
{
    check(i.size() == d_, "");
    check(halfDimProd_.size() == d_ + 1, "");

    unsigned long res = 0;

    for(int j = 0; j < d_; ++j)
    {
        const int k = i[j];
        check(k >= 0 && k <= nx_[j], "");

        res += (unsigned long)(k) * halfDimProd_[j + 1];
    }

    return res;
}

void
InitialValPDESolver::physicalCoords(const std::vector<int>& i, std::vector<double> *coords) const
{
    check(i.size() == d_, "");
    check(coords->size() == d_, "");

    check(i[0] >= -1 && i[0] <= nx_[0], "");
    (*coords)[0] = xMin_[0] + deltaX_[0] * (i[0] + nx0Starting_);

    for(int j = 1; j < d_; ++j)
    {
        check(i[j] >= -1 && i[j] <= nx_[j], "");
        (*coords)[j] = xMin_[j] + deltaX_[j] * i[j];
    }
}

void
InitialValPDESolver::physicalCoordsHalf(const std::vector<int>& i, std::vector<double> *coords) const
{
    check(i.size() == d_, "");
    check(coords->size() == d_, "");

    check(i[0] >= 0 && i[0] <= nx_[0], "");
    (*coords)[0] = xMin_[0] + deltaX_[0] * (i[0] + nx0Starting_) - deltaX_[0] / 2;

    for(int j = 1; j < d_; ++j)
    {
        check(i[j] >= 0 && i[j] <= nx_[j], "");
        (*coords)[j] = xMin_[j] + deltaX_[j] * i[j] - deltaX_[j]/ 2;
    }
}

void
InitialValPDESolver::increaseIndex(std::vector<int>& i, const std::vector<int>& rangeBegin, const std::vector<int>& rangeEnd) const
{
    check(i.size() == d_, "");
    check(rangeBegin.size() == d_, "");
    check(rangeEnd.size() == d_, "");

    for(int j = d_ - 1; j >= 0; --j)
    {
        const int nx = nx_[j];
        check(i[j] >= rangeBegin[j] && i[j] < rangeEnd[j], "");
        if(++i[j] < rangeEnd[j])
            return;
        if(j > 0)
            i[j] = rangeBegin[j];
    }
}


} // namespace Math

#endif

