#ifndef CIRCULARBOUNDARYCONDITION_HPP_
#define CIRCULARBOUNDARYCONDITION_HPP_

#include "AbstractCellPopulationBoundaryCondition.hpp"

#include "ChasteSerialization.hpp"
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/vector.hpp>

class CircularBoundaryCondition : public AbstractCellPopulationBoundaryCondition<2>
{
private:

    c_vector<double,2> mCentreOfCircle;
    double mRadiusOfCircle;

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive & archive, const unsigned int version)
    {
        archive & boost::serialization::base_object<AbstractCellPopulationBoundaryCondition<2> >(*this);
    }

public:

    CircularBoundaryCondition(AbstractCellPopulation<2>* pCellPopulation,
                              c_vector<double,2> centre,
                              double radius);

    const c_vector<double,2>& rGetCentreOfCircle() const;

    double GetRadiusOfCircle() const;

    void ImposeBoundaryCondition(const std::map<Node<2>*, c_vector<double,2> >& rOldLocations);

    bool VerifyBoundaryCondition();

    void OutputCellPopulationBoundaryConditionParameters(out_stream& rParamsFile);
};

#include "SerializationExportWrapper.hpp"
CHASTE_CLASS_EXPORT(CircularBoundaryCondition)

namespace boost
{
namespace serialization
{
template<class Archive>
inline void save_construct_data(
    Archive & ar, const CircularBoundaryCondition* t, const unsigned int file_version)
{
    const AbstractCellPopulation<2>* const p_cell_population = t->GetCellPopulation();
    ar << p_cell_population;

    c_vector<double,2> point = t->rGetCentreOfCircle();
    ar << point[0];
    ar << point[1];

    double radius = t->GetRadiusOfCircle();
    ar << radius;
}

template<class Archive>
inline void load_construct_data(
    Archive & ar, CircularBoundaryCondition* t, const unsigned int file_version)
{
    AbstractCellPopulation<2>* p_cell_population;
    ar >> p_cell_population;

    c_vector<double,2> point;
    ar >> point[0];
    ar >> point[1];

    double radius;
    ar >> radius;

    ::new(t)CircularBoundaryCondition(p_cell_population, point, radius);
}
}
}

#endif /*CIRCULARBOUNDARYCONDITION_HPP_*/
