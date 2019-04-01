// Automatically generated by generate_ops.py. DO NOT EDIT.

case SpvOpFunctionParameter: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    ip->code.push_back(InsnFunctionParameter{type, resultId});
    if(ip->verbose) {
        std::cout << "FunctionParameter";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << "\n";
    }
    break;
}

case SpvOpFunctionCall: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    uint32_t functionId = nextu();
    std::vector<uint32_t> operandId = restv();
    ip->code.push_back(InsnFunctionCall{type, resultId, functionId, operandId});
    if(ip->verbose) {
        std::cout << "FunctionCall";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " functionId ";
        std::cout << functionId;
        std::cout << " operandId ";
        for(int i = 0; i < operandId.size(); i++)
            std::cout << operandId[i] << " ";
        std::cout << "\n";
    }
    break;
}

case SpvOpLoad: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    uint32_t pointerId = nextu();
    uint32_t memoryAccess = nextu(NO_MEMORY_ACCESS_SEMANTIC);
    ip->code.push_back(InsnLoad{type, resultId, pointerId, memoryAccess});
    if(ip->verbose) {
        std::cout << "Load";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " pointerId ";
        std::cout << pointerId;
        std::cout << " memoryAccess ";
        std::cout << memoryAccess;
        std::cout << "\n";
    }
    break;
}

case SpvOpStore: {
    uint32_t pointerId = nextu();
    uint32_t objectId = nextu();
    uint32_t memoryAccess = nextu(NO_MEMORY_ACCESS_SEMANTIC);
    ip->code.push_back(InsnStore{pointerId, objectId, memoryAccess});
    if(ip->verbose) {
        std::cout << "Store";
        std::cout << " pointerId ";
        std::cout << pointerId;
        std::cout << " objectId ";
        std::cout << objectId;
        std::cout << " memoryAccess ";
        std::cout << memoryAccess;
        std::cout << "\n";
    }
    break;
}

case SpvOpAccessChain: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    uint32_t baseId = nextu();
    std::vector<uint32_t> indexesId = restv();
    ip->code.push_back(InsnAccessChain{type, resultId, baseId, indexesId});
    if(ip->verbose) {
        std::cout << "AccessChain";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " baseId ";
        std::cout << baseId;
        std::cout << " indexesId ";
        for(int i = 0; i < indexesId.size(); i++)
            std::cout << indexesId[i] << " ";
        std::cout << "\n";
    }
    break;
}

case SpvOpVectorShuffle: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    uint32_t vector1Id = nextu();
    uint32_t vector2Id = nextu();
    std::vector<uint32_t> componentsId = restv();
    ip->code.push_back(InsnVectorShuffle{type, resultId, vector1Id, vector2Id, componentsId});
    if(ip->verbose) {
        std::cout << "VectorShuffle";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " vector1Id ";
        std::cout << vector1Id;
        std::cout << " vector2Id ";
        std::cout << vector2Id;
        std::cout << " componentsId ";
        for(int i = 0; i < componentsId.size(); i++)
            std::cout << componentsId[i] << " ";
        std::cout << "\n";
    }
    break;
}

case SpvOpCompositeConstruct: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    std::vector<uint32_t> constituentsId = restv();
    ip->code.push_back(InsnCompositeConstruct{type, resultId, constituentsId});
    if(ip->verbose) {
        std::cout << "CompositeConstruct";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " constituentsId ";
        for(int i = 0; i < constituentsId.size(); i++)
            std::cout << constituentsId[i] << " ";
        std::cout << "\n";
    }
    break;
}

case SpvOpCompositeExtract: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    uint32_t compositeId = nextu();
    std::vector<uint32_t> indexesId = restv();
    ip->code.push_back(InsnCompositeExtract{type, resultId, compositeId, indexesId});
    if(ip->verbose) {
        std::cout << "CompositeExtract";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " compositeId ";
        std::cout << compositeId;
        std::cout << " indexesId ";
        for(int i = 0; i < indexesId.size(); i++)
            std::cout << indexesId[i] << " ";
        std::cout << "\n";
    }
    break;
}

case SpvOpConvertFToS: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    uint32_t floatValueId = nextu();
    ip->code.push_back(InsnConvertFToS{type, resultId, floatValueId});
    if(ip->verbose) {
        std::cout << "ConvertFToS";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " floatValueId ";
        std::cout << floatValueId;
        std::cout << "\n";
    }
    break;
}

case SpvOpConvertSToF: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    uint32_t signedValueId = nextu();
    ip->code.push_back(InsnConvertSToF{type, resultId, signedValueId});
    if(ip->verbose) {
        std::cout << "ConvertSToF";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " signedValueId ";
        std::cout << signedValueId;
        std::cout << "\n";
    }
    break;
}

case SpvOpFNegate: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    uint32_t operandId = nextu();
    ip->code.push_back(InsnFNegate{type, resultId, operandId});
    if(ip->verbose) {
        std::cout << "FNegate";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " operandId ";
        std::cout << operandId;
        std::cout << "\n";
    }
    break;
}

case SpvOpIAdd: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    uint32_t operand1Id = nextu();
    uint32_t operand2Id = nextu();
    ip->code.push_back(InsnIAdd{type, resultId, operand1Id, operand2Id});
    if(ip->verbose) {
        std::cout << "IAdd";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " operand1Id ";
        std::cout << operand1Id;
        std::cout << " operand2Id ";
        std::cout << operand2Id;
        std::cout << "\n";
    }
    break;
}

case SpvOpFAdd: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    uint32_t operand1Id = nextu();
    uint32_t operand2Id = nextu();
    ip->code.push_back(InsnFAdd{type, resultId, operand1Id, operand2Id});
    if(ip->verbose) {
        std::cout << "FAdd";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " operand1Id ";
        std::cout << operand1Id;
        std::cout << " operand2Id ";
        std::cout << operand2Id;
        std::cout << "\n";
    }
    break;
}

case SpvOpFSub: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    uint32_t operand1Id = nextu();
    uint32_t operand2Id = nextu();
    ip->code.push_back(InsnFSub{type, resultId, operand1Id, operand2Id});
    if(ip->verbose) {
        std::cout << "FSub";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " operand1Id ";
        std::cout << operand1Id;
        std::cout << " operand2Id ";
        std::cout << operand2Id;
        std::cout << "\n";
    }
    break;
}

case SpvOpFMul: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    uint32_t operand1Id = nextu();
    uint32_t operand2Id = nextu();
    ip->code.push_back(InsnFMul{type, resultId, operand1Id, operand2Id});
    if(ip->verbose) {
        std::cout << "FMul";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " operand1Id ";
        std::cout << operand1Id;
        std::cout << " operand2Id ";
        std::cout << operand2Id;
        std::cout << "\n";
    }
    break;
}

case SpvOpFDiv: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    uint32_t operand1Id = nextu();
    uint32_t operand2Id = nextu();
    ip->code.push_back(InsnFDiv{type, resultId, operand1Id, operand2Id});
    if(ip->verbose) {
        std::cout << "FDiv";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " operand1Id ";
        std::cout << operand1Id;
        std::cout << " operand2Id ";
        std::cout << operand2Id;
        std::cout << "\n";
    }
    break;
}

case SpvOpFMod: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    uint32_t operand1Id = nextu();
    uint32_t operand2Id = nextu();
    ip->code.push_back(InsnFMod{type, resultId, operand1Id, operand2Id});
    if(ip->verbose) {
        std::cout << "FMod";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " operand1Id ";
        std::cout << operand1Id;
        std::cout << " operand2Id ";
        std::cout << operand2Id;
        std::cout << "\n";
    }
    break;
}

case SpvOpVectorTimesScalar: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    uint32_t vectorId = nextu();
    uint32_t scalarId = nextu();
    ip->code.push_back(InsnVectorTimesScalar{type, resultId, vectorId, scalarId});
    if(ip->verbose) {
        std::cout << "VectorTimesScalar";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " vectorId ";
        std::cout << vectorId;
        std::cout << " scalarId ";
        std::cout << scalarId;
        std::cout << "\n";
    }
    break;
}

case SpvOpDot: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    uint32_t vector1Id = nextu();
    uint32_t vector2Id = nextu();
    ip->code.push_back(InsnDot{type, resultId, vector1Id, vector2Id});
    if(ip->verbose) {
        std::cout << "Dot";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " vector1Id ";
        std::cout << vector1Id;
        std::cout << " vector2Id ";
        std::cout << vector2Id;
        std::cout << "\n";
    }
    break;
}

case SpvOpLogicalNot: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    uint32_t operandId = nextu();
    ip->code.push_back(InsnLogicalNot{type, resultId, operandId});
    if(ip->verbose) {
        std::cout << "LogicalNot";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " operandId ";
        std::cout << operandId;
        std::cout << "\n";
    }
    break;
}

case SpvOpSelect: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    uint32_t conditionId = nextu();
    uint32_t object1Id = nextu();
    uint32_t object2Id = nextu();
    ip->code.push_back(InsnSelect{type, resultId, conditionId, object1Id, object2Id});
    if(ip->verbose) {
        std::cout << "Select";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " conditionId ";
        std::cout << conditionId;
        std::cout << " object1Id ";
        std::cout << object1Id;
        std::cout << " object2Id ";
        std::cout << object2Id;
        std::cout << "\n";
    }
    break;
}

case SpvOpIEqual: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    uint32_t operand1Id = nextu();
    uint32_t operand2Id = nextu();
    ip->code.push_back(InsnIEqual{type, resultId, operand1Id, operand2Id});
    if(ip->verbose) {
        std::cout << "IEqual";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " operand1Id ";
        std::cout << operand1Id;
        std::cout << " operand2Id ";
        std::cout << operand2Id;
        std::cout << "\n";
    }
    break;
}

case SpvOpSLessThan: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    uint32_t operand1Id = nextu();
    uint32_t operand2Id = nextu();
    ip->code.push_back(InsnSLessThan{type, resultId, operand1Id, operand2Id});
    if(ip->verbose) {
        std::cout << "SLessThan";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " operand1Id ";
        std::cout << operand1Id;
        std::cout << " operand2Id ";
        std::cout << operand2Id;
        std::cout << "\n";
    }
    break;
}

case SpvOpFOrdEqual: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    uint32_t operand1Id = nextu();
    uint32_t operand2Id = nextu();
    ip->code.push_back(InsnFOrdEqual{type, resultId, operand1Id, operand2Id});
    if(ip->verbose) {
        std::cout << "FOrdEqual";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " operand1Id ";
        std::cout << operand1Id;
        std::cout << " operand2Id ";
        std::cout << operand2Id;
        std::cout << "\n";
    }
    break;
}

case SpvOpFOrdLessThan: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    uint32_t operand1Id = nextu();
    uint32_t operand2Id = nextu();
    ip->code.push_back(InsnFOrdLessThan{type, resultId, operand1Id, operand2Id});
    if(ip->verbose) {
        std::cout << "FOrdLessThan";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " operand1Id ";
        std::cout << operand1Id;
        std::cout << " operand2Id ";
        std::cout << operand2Id;
        std::cout << "\n";
    }
    break;
}

case SpvOpFOrdGreaterThan: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    uint32_t operand1Id = nextu();
    uint32_t operand2Id = nextu();
    ip->code.push_back(InsnFOrdGreaterThan{type, resultId, operand1Id, operand2Id});
    if(ip->verbose) {
        std::cout << "FOrdGreaterThan";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " operand1Id ";
        std::cout << operand1Id;
        std::cout << " operand2Id ";
        std::cout << operand2Id;
        std::cout << "\n";
    }
    break;
}

case SpvOpFOrdLessThanEqual: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    uint32_t operand1Id = nextu();
    uint32_t operand2Id = nextu();
    ip->code.push_back(InsnFOrdLessThanEqual{type, resultId, operand1Id, operand2Id});
    if(ip->verbose) {
        std::cout << "FOrdLessThanEqual";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " operand1Id ";
        std::cout << operand1Id;
        std::cout << " operand2Id ";
        std::cout << operand2Id;
        std::cout << "\n";
    }
    break;
}

case SpvOpFOrdGreaterThanEqual: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    uint32_t operand1Id = nextu();
    uint32_t operand2Id = nextu();
    ip->code.push_back(InsnFOrdGreaterThanEqual{type, resultId, operand1Id, operand2Id});
    if(ip->verbose) {
        std::cout << "FOrdGreaterThanEqual";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " operand1Id ";
        std::cout << operand1Id;
        std::cout << " operand2Id ";
        std::cout << operand2Id;
        std::cout << "\n";
    }
    break;
}

case SpvOpPhi: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    std::vector<uint32_t> operandId = restv();
    ip->code.push_back(InsnPhi{type, resultId, operandId});
    if(ip->verbose) {
        std::cout << "Phi";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " operandId ";
        for(int i = 0; i < operandId.size(); i++)
            std::cout << operandId[i] << " ";
        std::cout << "\n";
    }
    break;
}

case SpvOpBranch: {
    uint32_t targetLabelId = nextu();
    ip->code.push_back(InsnBranch{targetLabelId});
    if(ip->verbose) {
        std::cout << "Branch";
        std::cout << " targetLabelId ";
        std::cout << targetLabelId;
        std::cout << "\n";
    }
    break;
}

case SpvOpBranchConditional: {
    uint32_t conditionId = nextu();
    uint32_t trueLabelId = nextu();
    uint32_t falseLabelId = nextu();
    std::vector<uint32_t> branchweightsId = restv();
    ip->code.push_back(InsnBranchConditional{conditionId, trueLabelId, falseLabelId, branchweightsId});
    if(ip->verbose) {
        std::cout << "BranchConditional";
        std::cout << " conditionId ";
        std::cout << conditionId;
        std::cout << " trueLabelId ";
        std::cout << trueLabelId;
        std::cout << " falseLabelId ";
        std::cout << falseLabelId;
        std::cout << " branchweightsId ";
        for(int i = 0; i < branchweightsId.size(); i++)
            std::cout << branchweightsId[i] << " ";
        std::cout << "\n";
    }
    break;
}

case SpvOpReturn: {
    ip->code.push_back(InsnReturn{});
    if(ip->verbose) {
        std::cout << "Return";
        std::cout << "\n";
    }
    break;
}

case SpvOpReturnValue: {
    uint32_t valueId = nextu();
    ip->code.push_back(InsnReturnValue{valueId});
    if(ip->verbose) {
        std::cout << "ReturnValue";
        std::cout << " valueId ";
        std::cout << valueId;
        std::cout << "\n";
    }
    break;
}

case SpvOpExtInst: {
    uint32_t type = nextu();
    uint32_t resultId = nextu();
    uint32_t ext = nextu();
    uint32_t opcode = nextu();
    if(ext == ip->ExtInstGLSL_std_450_id) {
        switch(opcode) {
case GLSLstd450FAbs: {
    uint32_t xId = nextu();
    ip->code.push_back(InsnGLSLstd450FAbs{type, resultId, xId});
    if(ip->verbose) {
        std::cout << "GLSLstd450FAbs";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " xId ";
        std::cout << xId;
        std::cout << "\n";
    }
    break;
}

case GLSLstd450Floor: {
    uint32_t xId = nextu();
    ip->code.push_back(InsnGLSLstd450Floor{type, resultId, xId});
    if(ip->verbose) {
        std::cout << "GLSLstd450Floor";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " xId ";
        std::cout << xId;
        std::cout << "\n";
    }
    break;
}

case GLSLstd450Sin: {
    uint32_t xId = nextu();
    ip->code.push_back(InsnGLSLstd450Sin{type, resultId, xId});
    if(ip->verbose) {
        std::cout << "GLSLstd450Sin";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " xId ";
        std::cout << xId;
        std::cout << "\n";
    }
    break;
}

case GLSLstd450Cos: {
    uint32_t xId = nextu();
    ip->code.push_back(InsnGLSLstd450Cos{type, resultId, xId});
    if(ip->verbose) {
        std::cout << "GLSLstd450Cos";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " xId ";
        std::cout << xId;
        std::cout << "\n";
    }
    break;
}

case GLSLstd450Pow: {
    uint32_t xId = nextu();
    uint32_t yId = nextu();
    ip->code.push_back(InsnGLSLstd450Pow{type, resultId, xId, yId});
    if(ip->verbose) {
        std::cout << "GLSLstd450Pow";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " xId ";
        std::cout << xId;
        std::cout << " yId ";
        std::cout << yId;
        std::cout << "\n";
    }
    break;
}

case GLSLstd450FMin: {
    uint32_t xId = nextu();
    uint32_t yId = nextu();
    ip->code.push_back(InsnGLSLstd450FMin{type, resultId, xId, yId});
    if(ip->verbose) {
        std::cout << "GLSLstd450FMin";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " xId ";
        std::cout << xId;
        std::cout << " yId ";
        std::cout << yId;
        std::cout << "\n";
    }
    break;
}

case GLSLstd450FMax: {
    uint32_t xId = nextu();
    uint32_t yId = nextu();
    ip->code.push_back(InsnGLSLstd450FMax{type, resultId, xId, yId});
    if(ip->verbose) {
        std::cout << "GLSLstd450FMax";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " xId ";
        std::cout << xId;
        std::cout << " yId ";
        std::cout << yId;
        std::cout << "\n";
    }
    break;
}

case GLSLstd450Length: {
    uint32_t xId = nextu();
    ip->code.push_back(InsnGLSLstd450Length{type, resultId, xId});
    if(ip->verbose) {
        std::cout << "GLSLstd450Length";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " xId ";
        std::cout << xId;
        std::cout << "\n";
    }
    break;
}

case GLSLstd450Distance: {
    uint32_t p0Id = nextu();
    uint32_t p1Id = nextu();
    ip->code.push_back(InsnGLSLstd450Distance{type, resultId, p0Id, p1Id});
    if(ip->verbose) {
        std::cout << "GLSLstd450Distance";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " p0Id ";
        std::cout << p0Id;
        std::cout << " p1Id ";
        std::cout << p1Id;
        std::cout << "\n";
    }
    break;
}

case GLSLstd450Cross: {
    uint32_t xId = nextu();
    uint32_t yId = nextu();
    ip->code.push_back(InsnGLSLstd450Cross{type, resultId, xId, yId});
    if(ip->verbose) {
        std::cout << "GLSLstd450Cross";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " xId ";
        std::cout << xId;
        std::cout << " yId ";
        std::cout << yId;
        std::cout << "\n";
    }
    break;
}

case GLSLstd450Normalize: {
    uint32_t xId = nextu();
    ip->code.push_back(InsnGLSLstd450Normalize{type, resultId, xId});
    if(ip->verbose) {
        std::cout << "GLSLstd450Normalize";
        std::cout << " type ";
        std::cout << type;
        std::cout << " resultId ";
        std::cout << resultId;
        std::cout << " xId ";
        std::cout << xId;
        std::cout << "\n";
    }
    break;
}

            default: {
                if(ip->throwOnUnimplemented) {
                    throw std::runtime_error("unimplemented GLSLstd450 opcode " + GLSLstd450OpcodeToString[opcode] + " (" + std::to_string(opcode) + ")");
                } else {
                    std::cout << "unimplemented GLSLstd450 opcode " << GLSLstd450OpcodeToString[opcode] << " (" << opcode << ")\n";
                    ip->hasUnimplemented = true;
                }
                break;
            }
        }
    } else {
        throw std::runtime_error("unimplemented instruction " + std::to_string(opcode) + " from extension set " + std::to_string(ext));
    }
    break;
}
