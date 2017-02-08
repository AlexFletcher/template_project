
#include "FarhadifarForceWithGrowingTargetAreas.hpp"
#include "AreaBasedCellCycleModel.hpp"

template<unsigned DIM>
FarhadifarForceWithGrowingTargetAreas<DIM>::FarhadifarForceWithGrowingTargetAreas()
   : AbstractForce<DIM>(),
     mAreaElasticityParameter(1.0), // These parameters are Case I in Farhadifar's paper
     mPerimeterContractilityParameter(0.04),
     mLineTensionParameter(0.12)
{
}

template<unsigned DIM>
FarhadifarForceWithGrowingTargetAreas<DIM>::~FarhadifarForceWithGrowingTargetAreas()
{
}

template<unsigned DIM>
void FarhadifarForceWithGrowingTargetAreas<DIM>::AddForceContribution(AbstractCellPopulation<DIM>& rCellPopulation)
{
    // Define some helper variables
    VertexBasedCellPopulation<DIM>* p_cell_population = static_cast<VertexBasedCellPopulation<DIM>*>(&rCellPopulation);
    unsigned num_nodes = p_cell_population->GetNumNodes();
    unsigned num_elements = p_cell_population->GetNumElements();

    // Begin by computing the area and perimeter of each element in the mesh, to avoid having to do this multiple times
    std::vector<double> element_areas(num_elements);
    std::vector<double> element_perimeters(num_elements);
    std::vector<double> target_areas(num_elements);
    for (typename VertexMesh<DIM,DIM>::VertexElementIterator elem_iter = p_cell_population->rGetMesh().GetElementIteratorBegin();
         elem_iter != p_cell_population->rGetMesh().GetElementIteratorEnd();
         ++elem_iter)
    {
        unsigned elem_index = elem_iter->GetIndex();
        element_areas[elem_index] = p_cell_population->rGetMesh().GetVolumeOfElement(elem_index);
        element_perimeters[elem_index] = p_cell_population->rGetMesh().GetSurfaceAreaOfElement(elem_index);

        CellPtr p_cell = p_cell_population->GetCellUsingLocationIndex(elem_index);
        assert(dynamic_cast<AreaBasedCellCycleModel*>(p_cell->GetCellCycleModel()) != NULL);

        double target_area = static_cast<AreaBasedCellCycleModel*>(p_cell->GetCellCycleModel())->GetTargetArea();
        target_areas[elem_index] = target_area;
    }

    // Iterate over vertices in the cell population
    for (unsigned node_index=0; node_index<num_nodes; node_index++)
    {
        Node<DIM>* p_this_node = p_cell_population->GetNode(node_index);

        /*
         * The force on this Node is given by the gradient of the total free
         * energy of the CellPopulation, evaluated at the position of the vertex. This
         * free energy is the sum of the free energies of all CellPtrs in
         * the cell population. The free energy of each CellPtr is comprised of three
         * terms - an area deformation energy, a perimeter deformation energy
         * and line tension energy.
         *
         * Note that since the movement of this Node only affects the free energy
         * of the CellPtrs containing it, we can just consider the contributions
         * to the free energy gradient from each of these CellPtrs.
         */
        c_vector<double, DIM> area_elasticity_contribution = zero_vector<double>(DIM);
        c_vector<double, DIM> perimeter_contractility_contribution = zero_vector<double>(DIM);
        c_vector<double, DIM> line_tension_contribution = zero_vector<double>(DIM);

        // Find the indices of the elements owned by this node
        std::set<unsigned> containing_elem_indices = p_cell_population->GetNode(node_index)->rGetContainingElementIndices();

        // Iterate over these elements
        for (std::set<unsigned>::iterator iter = containing_elem_indices.begin();
             iter != containing_elem_indices.end();
             ++iter)
        {
            // Get this element, its index and its number of nodes
            VertexElement<DIM, DIM>* p_element = p_cell_population->GetElement(*iter);
            unsigned elem_index = p_element->GetIndex();
            unsigned num_nodes_elem = p_element->GetNumNodes();

            // Find the local index of this node in this element
            unsigned local_index = p_element->GetNodeLocalIndex(node_index);

            // Add the force contribution from this cell's area elasticity (note the minus sign)
            c_vector<double, DIM> element_area_gradient =
                    p_cell_population->rGetMesh().GetAreaGradientOfElementAtNode(p_element, local_index);
            area_elasticity_contribution -= GetAreaElasticityParameter()*(element_areas[elem_index] -
                    target_areas[elem_index])*element_area_gradient;

            // Get the previous and next nodes in this element
            unsigned previous_node_local_index = (num_nodes_elem+local_index-1)%num_nodes_elem;
            Node<DIM>* p_previous_node = p_element->GetNode(previous_node_local_index);

            unsigned next_node_local_index = (local_index+1)%num_nodes_elem;
            Node<DIM>* p_next_node = p_element->GetNode(next_node_local_index);

            // Compute the line tension parameter for each of these edges - be aware that this is half of the actual
            // value for internal edges since we are looping over each of the internal edges twice
            double previous_edge_line_tension_parameter = GetLineTensionParameter(p_previous_node, p_this_node, *p_cell_population);
            double next_edge_line_tension_parameter = GetLineTensionParameter(p_this_node, p_next_node, *p_cell_population);

            // Compute the gradient of each these edges, computed at the present node
            c_vector<double, DIM> previous_edge_gradient =
                    -p_cell_population->rGetMesh().GetNextEdgeGradientOfElementAtNode(p_element, previous_node_local_index);
            c_vector<double, DIM> next_edge_gradient = p_cell_population->rGetMesh().GetNextEdgeGradientOfElementAtNode(p_element, local_index);

            // Add the force contribution from cell-cell and cell-boundary line tension (note the minus sign)
            line_tension_contribution -= previous_edge_line_tension_parameter*previous_edge_gradient +
                    next_edge_line_tension_parameter*next_edge_gradient;

            // Add the force contribution from this cell's perimeter contractility (note the minus sign)
            c_vector<double, DIM> element_perimeter_gradient = previous_edge_gradient + next_edge_gradient;
            perimeter_contractility_contribution -= GetPerimeterContractilityParameter()* element_perimeters[elem_index]*
                                                                                                     element_perimeter_gradient;
        }

        c_vector<double, DIM> force_on_node = area_elasticity_contribution + perimeter_contractility_contribution + line_tension_contribution;
        p_cell_population->GetNode(node_index)->AddAppliedForceContribution(force_on_node);
    }
}

template<unsigned DIM>
double FarhadifarForceWithGrowingTargetAreas<DIM>::GetLineTensionParameter(Node<DIM>* pNodeA, Node<DIM>* pNodeB, VertexBasedCellPopulation<DIM>& rVertexCellPopulation)
{
    // Find the indices of the elements owned by each node
    std::set<unsigned> elements_containing_nodeA = pNodeA->rGetContainingElementIndices();
    std::set<unsigned> elements_containing_nodeB = pNodeB->rGetContainingElementIndices();

    // Find common elements
    std::set<unsigned> shared_elements;
    std::set_intersection(elements_containing_nodeA.begin(),
                          elements_containing_nodeA.end(),
                          elements_containing_nodeB.begin(),
                          elements_containing_nodeB.end(),
                          std::inserter(shared_elements, shared_elements.begin()));

    // Check that the nodes have a common edge
    assert(!shared_elements.empty());

    // Since each internal edge is visited twice in the loop above, we have to use half the line tension parameter
    // for each visit.
    double line_tension_parameter_in_calculation = GetLineTensionParameter()/2.0;

    // If the edge corresponds to a single element, then the cell is on the boundary
    if (shared_elements.size() == 1)
    {
        line_tension_parameter_in_calculation = GetLineTensionParameter();
    }

    return line_tension_parameter_in_calculation;
}

template<unsigned DIM>
double FarhadifarForceWithGrowingTargetAreas<DIM>::GetAreaElasticityParameter()
{
    return mAreaElasticityParameter;
}

template<unsigned DIM>
double FarhadifarForceWithGrowingTargetAreas<DIM>::GetPerimeterContractilityParameter()
{
    return mPerimeterContractilityParameter;
}

template<unsigned DIM>
double FarhadifarForceWithGrowingTargetAreas<DIM>::GetLineTensionParameter()
{
    return mLineTensionParameter;
}

template<unsigned DIM>
void FarhadifarForceWithGrowingTargetAreas<DIM>::SetAreaElasticityParameter(double areaElasticityParameter)
{
    mAreaElasticityParameter = areaElasticityParameter;
}

template<unsigned DIM>
void FarhadifarForceWithGrowingTargetAreas<DIM>::SetPerimeterContractilityParameter(double perimeterContractilityParameter)
{
    mPerimeterContractilityParameter = perimeterContractilityParameter;
}

template<unsigned DIM>
void FarhadifarForceWithGrowingTargetAreas<DIM>::SetLineTensionParameter(double lineTensionParameter)
{
    mLineTensionParameter = lineTensionParameter;
}

template<unsigned DIM>
void FarhadifarForceWithGrowingTargetAreas<DIM>::OutputForceParameters(out_stream& rParamsFile)
{
    *rParamsFile << "\t\t\t<AreaElasticityParameter>" << mAreaElasticityParameter << "</AreaElasticityParameter>\n";
    *rParamsFile << "\t\t\t<PerimeterContractilityParameter>" << mPerimeterContractilityParameter << "</PerimeterContractilityParameter>\n";
    *rParamsFile << "\t\t\t<LineTensionParameter>" << mLineTensionParameter << "</LineTensionParameter>\n";

    // Call method on direct parent class
    AbstractForce<DIM>::OutputForceParameters(rParamsFile);
}

// Explicit instantiation
template class FarhadifarForceWithGrowingTargetAreas<1>;
template class FarhadifarForceWithGrowingTargetAreas<2>;
template class FarhadifarForceWithGrowingTargetAreas<3>;

// Serialization for Boost >= 1.36
#include "SerializationExportWrapperForCpp.hpp"
EXPORT_TEMPLATE_CLASS_SAME_DIMS(FarhadifarForceWithGrowingTargetAreas)