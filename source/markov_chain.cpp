#include <fstream>
#include <string>
#include <sstream>
#include <limits>
#include <algorithm>
#include <utility>
#include <cmath>

#include <macros.hpp>
#include <exception_handler.hpp>
#include <cubic_spline.hpp>
#include <gauss_smooth.hpp>
#include <progress_meter.hpp>
#include <markov_chain.hpp>
#include <numerics.hpp>

void
Posterior1D::addPoint(double x, double prob, double like, double errMean, double errVar)
{
    points_.push_back(x);
    probs_.push_back(prob);
    likes_.push_back(like);
    errMean_.push_back(errMean);
    errVar_.push_back(errVar);

    if(x < min_)
        min_ = x;
    if(x > max_)
        max_ = x;

    if(like < minLike_)
    {
        minLike_ = like;
        maxLikePoint_ = x;
    }
}

namespace
{

struct LessPairFirst
{
    bool operator() (const std::pair<double, double>& a, const std::pair<double, double>& b)
    {
        return a.first < b.first;
    }
};

}

void
Posterior1D::generate(SmoothingMethod method, double scale)
{
    method_ = method;

    check(points_.size() >= 2, "at least 2 different data points need to be added before generating");
    check(points_.size() == probs_.size(), "");
    check(max_ > min_, "at least 2 different data points need to be added before generating");

    check(scale >= 0, "invalid scale " << scale);

    std::vector<std::pair<double, double> > pointsSorted(points_.size());
    double totalWeight = 0;
    for(int i = 0; i < points_.size(); ++i)
    {
        pointsSorted[i].first = points_[i];
        pointsSorted[i].second = probs_[i];
        totalWeight += probs_[i];
    }

    LessPairFirst less;

    std::sort(pointsSorted.begin(), pointsSorted.end(), less);

    int q1 = 0, q3 = pointsSorted.size() - 1;
    double cumulWeight = 0;
    for(int i = 0; i < pointsSorted.size(); ++i)
    {
        cumulWeight += pointsSorted[i].second;
        if(cumulWeight >= 0.25 * totalWeight && q1 == 0)
            q1 = i;

        if(cumulWeight >= 0.75 * totalWeight && q3 == pointsSorted.size() - 1)
            q3 = i;
    }

    const double iqr = pointsSorted[q3].first - pointsSorted[q1].first;
    const double myBinSize = 2 * iqr * std::pow(double(pointsSorted.size()), -1.0 / 3.0);

    int resolution;
    if(myBinSize == 0 || pointsSorted.size() < 5)
        resolution = 1;
    else
        resolution = (max_ - min_) / myBinSize;

    if(scale == 0)
    {
        check(iqr > 0, "cannot determine smmoothing scale because iqr = 0");
        scale = iqr / 8;
    }

    check(resolution > 0, "");

    std::vector<double> x(resolution + 2), y(resolution + 2, 0), vars(resolution + 2, 0);
    const double d = (max_ - min_) / resolution;
    x[0] = min_;
    x[resolution + 1] = max_;
    for(int i = 0; i < resolution; ++i)
        x[i + 1] = min_ + d * i + d / 2;

    mean_ = 0;
    double totalP = 0;
    for(unsigned long i = 0; i < points_.size(); ++i)
    {
        const double p = points_[i]; 
        check(p >= min_, "");
        int k = (int)std::floor((p - min_) / d);
        check(k >= 0, "");
        if(k >= resolution)
            k = resolution - 1;

        y[k + 1] += probs_[i];
        mean_ += p * probs_[i];
        totalP += probs_[i];
        vars[k + 1] += probs_[i] * errVar_[i] / 4; // divide by 4 since the variance is for -2logL, want logL
    }

    double varNorm = 0;

    //output_screen_clean("DISTRIBUTION HISTOGRAM:" << std::endl);
    for(int i = 0; i < vars.size(); ++i)
    {
        if(y[i] != 0)
        {
            vars[i] = std::sqrt(vars[i]);

            // re-weighing the points so that each point has a weight of 1 on average
            vars[i] /= std::sqrt(points_.size() / totalP);
        }
        else
        {
            check(vars[i] == 0, "");
        }
        varNorm += vars[i] * vars[i];

        //output_screen_clean(x[i] << "\t" << y[i] << "\t" << vars[i] << std::endl);
    }

    varNorm = std::sqrt(varNorm);

    // to make sure edges are smooth
    y[0] = y[1];
    y[resolution + 1] = y[resolution];

    check(totalP, "");
    mean_ /= totalP;

    if(smooth_)
    {
        check(cumulInv_, "");
        delete smooth_;
        delete cumulInv_;
    }

    switch(method)
    {
    case GAUSSIAN_SMOOTHING:
        smooth_ = new Math::GaussSmooth(x, y, scale, &vars);
        break;
    case SPLINE_SMOOTHING:
        smooth_ = new Math::CubicSpline(x, y);
        break;
    default:
        check(false, "");
        break;
    }

    const int N = 100 * resolution;
    const double delta = (x[x.size() - 1] - x[0]) / N;
    cumulInv_ = new Math::TableFunction<double, double>;
    double maxVal = 0, maxX;
    norm_ = 0;
    (*cumulInv_)[0] = 0;
    for(int i = 0; i <= N; ++i)
    {
        double v = x[0] + i * delta;
        
        if(i == N)
            v = x[x.size() - 1];

        check(v <= x[x.size() - 1], "");

        double y = smooth_->evaluate(v);
        if(y < 0)
            y = 0;

        if(y > maxVal)
        {
            maxVal = y;
            maxX = v;
        }

        norm_ += y * delta;
        (*cumulInv_)[norm_] = v;
    }

    deltaNorm_ = varNorm / totalP * norm_;

    points_.clear();
    probs_.clear();
}

double
Posterior1D::evaluateError(double x) const
{
    check(smooth_, "not generated");

    if(method_ != GAUSSIAN_SMOOTHING)
        return 0;

    Math::GaussSmooth* gs = (Math::GaussSmooth*) smooth_;
    const double a = gs->evaluate(x), deltaA = gs->evaluateError(x);
    return a / norm_ * std::sqrt(deltaA * deltaA / (a * a) + deltaNorm_ * deltaNorm_ / (norm_ * norm_));
}

double
Posterior1D::peak() const
{
    const int nPoints = 10000;
    const double delta = (max() - min()) / nPoints;
    double peakVal = 0, x = min();
    for(int i = 0; i <= nPoints; ++i)
    {
        double t = min() + i * delta;
        if(i == nPoints)
            t = max();
        const double y = evaluate(t);
        if(y > peakVal)
        {
            peakVal = y;
            x = t;
        }
    }

    return x;
}

void
Posterior1D::writeIntoFile(const char* fileName, int n, bool includeError) const
{
    check(n >= 2, "invalid number of points " << n << ", should be at least 2.");
    std::ofstream out(fileName);
    StandardException exc;
    if(!out)
    {
        std::stringstream exceptionStr;
        exceptionStr << "Cannot write into file " << fileName << ".";
        exc.set(exceptionStr.str());
        throw exc;
    }

    const double delta = (max() - min()) / n;
    for(int i = 0; i <= n; ++i)
    {
        double x = min() + i * delta;
        if(i == n)
            x = max();

        check(x >= min() && x <= max(), "");

        out << x << '\t' << evaluate(x);
        if(includeError)
            out << '\t' << evaluateError(x);

        out << std::endl;
    }
    out.close();
}

void
Posterior2D::addPoint(double x1, double x2, double prob, double like)
{
    points1_.push_back(x1);
    points2_.push_back(x2);
    probs_.push_back(prob);

    if(x1 < min1_)
        min1_ = x1;
    if(x2 < min2_)
        min2_ = x2;
    if(x1 > max1_)
        max1_ = x1;
    if(x2 > max2_)
        max2_ = x2;

    if(like < minLike_)
    {
        minLike_ = like;
        maxLikePoint1_ = x1;
        maxLikePoint2_ = x2;
    }
}

void
Posterior2D::generate(double scale1, double scale2)
{
    std::vector<std::pair<double, double> > points1Sorted(points1_.size());
    double totalWeight = 0;
    for(int i = 0; i < points1_.size(); ++i)
    {
        points1Sorted[i].first = points1_[i];
        points1Sorted[i].second = probs_[i];
        totalWeight += probs_[i];
    }

    LessPairFirst less;

    std::sort(points1Sorted.begin(), points1Sorted.end(), less);

    int q1 = 0, q3 = points1Sorted.size() - 1;
    double cumulWeight = 0;
    for(int i = 0; i < points1Sorted.size(); ++i)
    {
        cumulWeight += points1Sorted[i].second;
        if(cumulWeight >= 0.25 * totalWeight && q1 == 0)
            q1 = i;

        if(cumulWeight >= 0.75 * totalWeight && q3 == points1Sorted.size() - 1)
            q3 = i;
    }

    const double iqr1 = points1Sorted[q3].first - points1Sorted[q1].first;
    const double myBinSize1 = 2 * iqr1 * std::pow(double(points1Sorted.size()), -1.0 / 3.0);

    int res1, res2;
    if(myBinSize1 == 0 || points1Sorted.size() < 5)
        res1 = 1;
    else
        res1 = (max1_ - min1_) / myBinSize1;

    if(scale1 == 0)
    {
        check(iqr1 > 0, "cannot determine smmoothing scale because iqr = 0");
        scale1 = iqr1 / 8;
    }

    std::vector<std::pair<double, double> > points2Sorted(points2_.size());
    for(int i = 0; i < points1_.size(); ++i)
    {
        points2Sorted[i].first = points2_[i];
        points2Sorted[i].second = probs_[i];
    }
    
    check(points2Sorted.size() == points1Sorted.size(), "");

    std::sort(points2Sorted.begin(), points2Sorted.end(), less);

    q1 = 0, q3 = points2Sorted.size() - 1;
    cumulWeight = 0;
    for(int i = 0; i < points2Sorted.size(); ++i)
    {
        cumulWeight += points2Sorted[i].second;
        if(cumulWeight >= 0.25 * totalWeight && q1 == 0)
            q1 = i;

        if(cumulWeight >= 0.75 * totalWeight && q3 == points2Sorted.size() - 1)
            q3 = i;
    }

    const double iqr2 = points2Sorted[q3].first - points2Sorted[q1].first;
    const double myBinSize2 = 2 * iqr2 * std::pow(double(points2Sorted.size()), -1.0 / 3.0);

    if(myBinSize2 == 0 || points2Sorted.size() < 5)
        res2 = 1;
    else
        res2 = (max2_ - min2_) / myBinSize2;

    if(scale2 == 0)
    {
        check(iqr2 > 0, "cannot determine smmoothing scale because iqr = 0");
        scale2 = iqr2 / 8;
    }

    check(res1 > 0, "");
    check(res2 > 0, "");

    std::vector<double> x1(res1 + 2), x2(res2 + 2);
    std::vector<std::vector<double> > y(res1 + 2);
    for(int i = 0; i < res1 + 2; ++i)
        y[i].resize(res2 + 2, 0);

    const double d1 = (max1_ - min1_) / res1;
    x1[0] = min1_;
    x1[res1 + 1] = max1_;
    for(int i = 0; i < res1; ++i)
        x1[i + 1] = min1_ + d1 * i + d1 / 2;

    const double d2 = (max2_ - min2_) / res2;
    x2[0] = min2_;
    x2[res2 + 1] = max2_;
    for(int i = 0; i < res2; ++i)
        x2[i + 1] = min2_ + d2 * i + d2 / 2;

    for(unsigned long i = 0; i < points1_.size(); ++i)
    {
        const double p1 = points1_[i];
        const double p2 = points2_[i];
        check(p1 >= min1_, "");
        check(p2 >= min2_, "");
        int k1 = (int)std::floor((p1 - min1_) / d1);
        int k2 = (int)std::floor((p2 - min2_) / d2);
        check(k1 >= 0, "");
        check(k2 >= 0, "");
        if(k1 >= res1)
            k1 = res1 - 1;
        if(k2 >= res2)
            k2 = res2 - 1;

        y[k1 + 1][k2 + 1] += probs_[i];
    }

    // to make sure edges are smooth
    for(int i = 0; i < y.size(); ++i)
    {
        y[i][0] = y[i][1];
        y[i][res2 + 1] = y[i][res2];
    }

    for(int i = 0; i < res2 + 2; ++i)
    {
        y[0][i] = y[1][i];
        y[res1 + 1][i] = y[res1][i];
    }

    if(smooth_)
    {
        check(cumulInv_, "");

        delete smooth_;
        delete cumulInv_;
    }

    smooth_ = new Math::GaussSmooth2D(x1, x2, y, scale1, scale2);

    const int N1 = 1000, N2 = 1000;
    const double delta1 = (x1[x1.size() - 1] - x1[0]) / N1;
    const double delta2 = (x2[x2.size() - 1] - x2[0]) / N2;

    std::vector<double> probs;
    norm_ = 0;
    output_screen("Sampling the 2D distribution..." << std::endl);
    ProgressMeter met((N1 + 1) * (N2 + 1));
    for(int i = 0; i <= N1; ++i)
    {
        double v1 = x1[0] + i * delta1;
        if(i == N1)
            v1 = x1[x1.size() - 1];

        check(v1 <= x1[x1.size() - 1], "");
        
        for(int j = 0; j <= N2; ++j)
        {
            double v2 = x2[0] + j * delta2;
            if(j == N2)
                v2 = x2[x2.size() - 1];

            check(v2 <= x2[x2.size() - 1], "");

            double y = smooth_->evaluate(v1, v2);
            probs.push_back(y);
            norm_ += y * delta1 * delta2;
            met.advance();
        }
    }
    output_screen("OK" << std::endl);

    check(norm_ > 0, "");

    std::sort(probs.begin(), probs.end());
    cumulInv_ = new Math::TableFunction<double, double>;

    double total = 0;
    int index = probs.size() - 1;
    while(total < 1 && index >= 0)
    {
        const double currentP = probs[index] / norm_;
        (*cumulInv_)[total] = currentP;
        total += currentP * delta1 * delta2;
        --index;
    }
    (*cumulInv_)[1] = 0;

    points1_.clear();
    points2_.clear();
    probs_.clear();
}

bool lessMarkovChainElementPointer(MarkovChain::Element* i, MarkovChain::Element* j)
{
    return (*i) < (*j);
}

MarkovChain::MarkovChain(const char* fileName, unsigned long burnin, unsigned int thin, const char *errorLogFileNameBase, int nError)
{
    if(errorLogFileNameBase)
        readErrorFiles(nError, errorLogFileNameBase);

    minLike_ = std::numeric_limits<double>::max();
    addFile(fileName, burnin, thin);
}

MarkovChain::MarkovChain(int nChains, const char* fileNameRoot, unsigned long burnin, unsigned int thin, const char *errorLogFileNameBase)
{
    check(nChains > 0, "need at least 1 chain");

    if(errorLogFileNameBase)
        readErrorFiles(nChains, errorLogFileNameBase);

    minLike_ = std::numeric_limits<double>::max();

    std::vector<Element*> bigChain;
    double maxP = std::numeric_limits<double>::min();
    for(int i = 0; i < nChains; ++i)
    {
        std::stringstream fileName;
        fileName << fileNameRoot;
        if(nChains > 1)
            fileName << '_' << i;
        fileName << ".txt";
        double thisMaxP;
        readFile(fileName.str().c_str(), burnin, thin, bigChain, thisMaxP);
        if(thisMaxP > maxP)
            maxP = thisMaxP;
    }

    double minP = maxP / bigChain.size() / 1000;
    filterChain(bigChain, minP);

    output_screen("Sorting the chain..." << std::endl);
    std::sort(chain_.begin(), chain_.end(), lessMarkovChainElementPointer);
    output_screen("OK" << std::endl);
}

MarkovChain::~MarkovChain()
{
    for(unsigned long i = 0; i < chain_.size(); ++i)
        delete chain_[i];
}

void
MarkovChain::addFile(const char* fileName, unsigned long burnin, unsigned int thin)
{
    std::vector<Element*> bigChain;
    double maxP;
    readFile(fileName, burnin, thin, bigChain, maxP);
    double minP = maxP / bigChain.size() / 1000;
    filterChain(bigChain, minP);

    output_screen("Sorting the chain..." << std::endl);
    std::sort(chain_.begin(), chain_.end(), lessMarkovChainElementPointer);
    output_screen("OK" << std::endl);
}

void
MarkovChain::readFile(const char* fileName, unsigned long burnin, unsigned int thin, std::vector<Element*>& bigChain, double& maxP)
{
    check(thin > 0, "thin factor cannot be 0");

    StandardException exc;
    std::ifstream in(fileName);

    if(!in)
    {
        std::stringstream exceptionStr;
        exceptionStr << "Cannot open input file " << fileName << ".";
        exc.set(exceptionStr.str());
        throw exc;
    }

    output_screen("Reading the chain from file " << fileName << "..." << std::endl);
    nParams_ = -1;
    unsigned long line = 0;
    maxP = std::numeric_limits<double>::min();

    int notFound = 0, found = 0;

    while(!in.eof())
    {
        std::string s;
        std::getline(in, s);
        if(s == "")
            break;

        std::stringstream str(s);
        Element* elem = new Element;
        str >> elem->prob >> elem->like;

        elem->errMean = 0;
        elem->errVar = 0;

        if(elem->prob > maxP)
            maxP = elem->prob;

        if(elem->like < minLike_)
            minLike_ = elem->like;

        while(!str.eof())
        {
            double val = std::numeric_limits<double>::min();
            str >> val;
            if(val == std::numeric_limits<double>::min())
                break;

            elem->params.push_back(val);
        }

        if(nParams_ == -1)
            nParams_ = elem->params.size();

        else if (nParams_ != elem->params.size())
        {
            std::stringstream exceptionStr;
            exceptionStr << "Invalid chain file " << fileName << ". There are " << elem->params.size() << " parameters on line " << line << " while the previous lines had " << nParams_ << " parameters.";
            exc.set(exceptionStr.str());
            throw exc;
        }

        if(line >= burnin && (line - burnin) % thin == 0)
            bigChain.push_back(elem);

        if(!errors_.empty())
        {
            ErrorEntry err;
            err.like = elem->like - 0.05;

            std::vector<ErrorEntry>::const_iterator it = std::lower_bound(errors_.begin(), errors_.end(), err);
            err.like = elem->like + 0.05;
            std::vector<ErrorEntry>::const_iterator end = std::lower_bound(errors_.begin(), errors_.end(), err);

            while(it != end)
            {
                bool equal = true;
                for(int i = 0; i < elem->params.size(); ++i)
                {
                    if(!Math::areEqual(elem->params[i], it->params[i], 1e-5))
                    {
                        equal = false;
                        break;
                    }
                }

                if(equal)
                {
                    elem->errMean = it->mean;
                    elem->errVar = it->var;
                    ++found;
                    break;
                }
                ++it;
            }
            if(it == end)
                ++notFound;
        }

        ++line;
    }
    output_screen("OK" << std::endl);
    output_screen("Successfully read the chain. It has " << bigChain.size() << " elements, " << nParams_ << " parameters." << std::endl);

    if(!errors_.empty())
    {
        output_screen("Entries found in the error log: " << found << " not found: " << notFound << std::endl);
    }
}

void
MarkovChain::filterChain(std::vector<Element*>& bigChain, double minP)
{
    output_screen("Filtering the chain..." << std::endl);
    ProgressMeter meter(bigChain.size());
    unsigned long left = 0;
    for(unsigned long i = 0; i < bigChain.size(); ++i)
    {
        Element* e = bigChain[i];
        if(e->prob < minP)
            delete e;
        else
        {
            chain_.push_back(e);
            ++left;
        }

        meter.advance();
    }
    output_screen("OK" << std::endl);
    output_screen(left << " elements left after filtering!" << std::endl);
}

Posterior1D*
MarkovChain::posterior(int paramIndex, Posterior1D::SmoothingMethod method, double scale) const
{
    check(paramIndex >= 0 && paramIndex < nParams_, "invalid parameter index " << paramIndex);
    Posterior1D* post = new Posterior1D;

    for(unsigned long i = 0; i < chain_.size(); ++i)
        post->addPoint(chain_[i]->params[paramIndex], chain_[i]->prob, chain_[i]->like, chain_[i]->errMean, chain_[i]->errVar);

    post->generate(method, scale);
    return post;
}

Posterior2D*
MarkovChain::posterior(int paramIndex1, int paramIndex2, double scale1, double scale2) const
{
    check(paramIndex1 >= 0 && paramIndex1 < nParams_, "invalid parameter index " << paramIndex1);
    check(paramIndex2 >= 0 && paramIndex2 < nParams_, "invalid parameter index " << paramIndex2);

    Posterior2D* post = new Posterior2D;
    for(unsigned long i = 0; i < chain_.size(); ++i)
        post->addPoint(chain_[i]->params[paramIndex1], chain_[i]->params[paramIndex2], chain_[i]->prob, chain_[i]->like);

    post->generate(scale1, scale2);
    return post;
}

void
Posterior2D::writeIntoFile(const char* fileName, int n) const
{
    check(n >= 2, "invalid number of points " << n << ", should be at least 2.");
    std::ofstream out(fileName);
    StandardException exc;
    if(!out)
    {
        std::stringstream exceptionStr;
        exceptionStr << "Cannot write into file " << fileName << ".";
        exc.set(exceptionStr.str());
        throw exc;
    }

    const double delta1 = (max1() - min1()) / n, delta2 = (max2() - min2()) / n;
    std::vector<double> x, y;
    for(int i = 0; i <= n; ++i)
    {
        double xVal = min1() + i * delta1, yVal = min2() + i * delta2;
        if(i == n)
        {
            xVal = max1();
            yVal = max2();
        }
        x.push_back(xVal);
        y.push_back(yVal);
    }
    out << x[0];
    for(int i = 1; i <= n; ++i)
        out << ' ' << x[i];

    out << std::endl;

    out << y[0];
    for(int i = 1; i <= n; ++i)
        out << ' ' << y[i];
    out << std::endl;
    
    for(int i = 0; i <= n; ++i)
    {
        for(int j = 0; j <= n; ++j)
        {
            double val = evaluate(x[i], y[j]);
            out << val;
            if(j != n)
                out << ' ';
        }
        out << std::endl;
    }
    out.close();
}

void
MarkovChain::getRange(std::vector<Element*>& container, double pUpper, double pLower) const
{
    check(pUpper >= 0 && pUpper <= 1, "invalid probability " << pUpper << ", should be between 0 and 1");
    check(pLower >= 0 && pLower <= pUpper, "invalid lower probability " << pLower << ", should be between 0 and " << pUpper);
    container.clear();

    if(pUpper == 0)
        return;

    if(pLower == 1)
    {
        container.insert(container.end(), chain_.begin(), chain_.end());
        return;
    }

    double total = 0;
    std::vector<Element*>::const_iterator it = chain_.begin();
    while(total <= pUpper && it != chain_.end())
    {
        total += (*it)->prob;
        if(total > pLower)
            container.push_back(*it);
        ++it;
    }
}

void
MarkovChain::readErrorFiles(int nError, const char *fileNameBase)
{
    StandardException exc;
    for(int i = 0; i < nError; ++i)
    {
        std::stringstream fileName;
        fileName << fileNameBase;
        if(nError > 1)
            fileName << "_" << i;
        fileName << ".txt";
        std::ifstream in(fileName.str().c_str());
        if(!in)
        {
            std::stringstream exceptionStr;
            exceptionStr << "Cannot open input file " << fileName.str() << ".";
            exc.set(exceptionStr.str());
            throw exc;
        }
        output_screen("Reading error file " << fileName.str() << "..." << std::endl);
        int nPar = -1;
        while(!in.eof())
        {
            std::string s;
            std::getline(in, s);
            if(s == "")
                break;

            std::stringstream str(s);
            ErrorEntry e;
            while(!str.eof())
            {
                double val = std::numeric_limits<double>::min();
                str >> val;
                if(val == std::numeric_limits<double>::min())
                    break;

                e.params.push_back(val);
            }
            if(e.params.size() < 6)
            {
                std::stringstream exceptionStr;
                exceptionStr << "Invalid error log file " << fileName.str() << ". Each line should contain at least 6 elements.";
                exc.set(exceptionStr.str());
                throw exc;
            }
            e.var = e.params.back();
            e.params.pop_back();
            e.mean = e.params.back();
            e.params.pop_back();
            e.params.pop_back();
            e.params.pop_back();
            e.like = e.params.back();
            e.params.pop_back();
            if(nPar == -1)
            {
                nPar = e.params.size();
                output_screen("The number of parameters is " << nPar << std::endl);
            }
            else if(nPar != e.params.size())
            {
                std::stringstream exceptionStr;
                exceptionStr << "Invalid error log file " << fileName.str() << ". Inconsistency in number of parameters between different rows.";
                exc.set(exceptionStr.str());
                throw exc;
            }

            errors_.push_back(e);
        }
        output_screen("OK" << std::endl);
    }
    std::sort(errors_.begin(), errors_.end());
    output_screen("Successfully read all of error files. A total of " << errors_.size() << " elements read." << std::endl);
}

