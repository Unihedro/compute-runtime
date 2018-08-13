/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/helpers/file_io.h"
#include "runtime/program/program.h"
#include "runtime/helpers/string.h"
#include "unit_tests/helpers/test_files.h"
#include "unit_tests/mocks/mock_program.h"
#include "gtest/gtest.h"
#include <cstring>

using namespace OCLRT;

class ProcessElfBinaryTests : public ::testing::Test {
  public:
    void SetUp() override {
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        program = std::make_unique<MockProgram>(*executionEnvironment);
    }

    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    std::unique_ptr<MockProgram> program;
};

TEST_F(ProcessElfBinaryTests, NullBinary) {
    uint32_t binaryVersion;
    cl_int retVal = program->processElfBinary(nullptr, 0, binaryVersion);

    EXPECT_EQ(CL_INVALID_BINARY, retVal);
    EXPECT_NE(0u, binaryVersion);
}

TEST_F(ProcessElfBinaryTests, InvalidBinary) {
    uint32_t binaryVersion;
    char pBinary[] = "thisistotallyinvalid\0";
    size_t binarySize = strnlen_s(pBinary, 21);
    cl_int retVal = program->processElfBinary(pBinary, binarySize, binaryVersion);

    EXPECT_EQ(CL_INVALID_BINARY, retVal);
    EXPECT_NE(0u, binaryVersion);
}

TEST_F(ProcessElfBinaryTests, ValidBinary) {
    uint32_t binaryVersion;
    void *pBinary = nullptr;
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "CopyBuffer_simd8_", ".bin");

    size_t binarySize = loadDataFromFile(filePath.c_str(), pBinary);
    cl_int retVal = program->processElfBinary(pBinary, binarySize, binaryVersion);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, memcmp(pBinary, program->elfBinary.data(), binarySize));
    EXPECT_NE(0u, binaryVersion);
    deleteDataReadFromFile(pBinary);
}

TEST_F(ProcessElfBinaryTests, ValidSpirvBinary) {
    //clCreateProgramWithIL => SPIR-V stored as source code
    const uint32_t spirvBinary[2] = {0x03022307, 0x07230203};
    size_t spirvBinarySize = sizeof(spirvBinary);
    auto isSpirV = Program::isValidSpirvBinary(spirvBinary, spirvBinarySize);
    EXPECT_TRUE(isSpirV);

    //clCompileProgram => SPIR-V stored as IR binary
    program->isSpirV = true;
    program->storeIrBinary(spirvBinary, spirvBinarySize, true);
    program->programBinaryType = CL_PROGRAM_BINARY_TYPE_LIBRARY;
    EXPECT_NE(nullptr, program->irBinary);
    EXPECT_NE(0u, program->irBinarySize);
    EXPECT_TRUE(program->getIsSpirV());

    //clGetProgramInfo => SPIR-V stored as ELF binary
    cl_int retVal = program->resolveProgramBinary();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(program->elfBinary.empty());
    EXPECT_NE(0u, program->elfBinarySize);

    //use ELF reader to parse and validate ELF binary
    CLElfLib::CElfReader elfReader(program->elfBinary);
    const CLElfLib::SElf64Header *elf64Header = elfReader.getElfHeader();
    ASSERT_NE(nullptr, elf64Header);
    EXPECT_EQ(elf64Header->Type, CLElfLib::E_EH_TYPE::EH_TYPE_OPENCL_LIBRARY);

    //check if ELF binary contains section SH_TYPE_SPIRV
    bool hasSpirvSection = false;
    for (const auto &elfSectionHeader : elfReader.getSectionHeaders()) {
        if (elfSectionHeader.Type == CLElfLib::E_SH_TYPE::SH_TYPE_SPIRV) {
            hasSpirvSection = true;
            break;
        }
    }
    EXPECT_TRUE(hasSpirvSection);

    //clCreateProgramWithBinary => new program should recognize SPIR-V binary
    program->isSpirV = false;
    uint32_t elfBinaryVersion;
    auto pElfBinary = std::unique_ptr<char>(new char[program->elfBinarySize]);
    memcpy_s(pElfBinary.get(), program->elfBinarySize, program->elfBinary.data(), program->elfBinarySize);
    retVal = program->processElfBinary(pElfBinary.get(), program->elfBinarySize, elfBinaryVersion);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_TRUE(program->getIsSpirV());
}

unsigned int BinaryTypeValues[] = {
    CL_PROGRAM_BINARY_TYPE_EXECUTABLE,
    CL_PROGRAM_BINARY_TYPE_LIBRARY,
    CL_PROGRAM_BINARY_TYPE_COMPILED_OBJECT};

class ProcessElfBinaryTestsWithBinaryType : public ::testing::TestWithParam<unsigned int> {
  public:
    void SetUp() override {
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        program = std::make_unique<MockProgram>(*executionEnvironment);
    }

    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    std::unique_ptr<MockProgram> program;
};

TEST_P(ProcessElfBinaryTestsWithBinaryType, GivenBinaryTypeWhenResolveProgramThenProgramIsProperlyResolved) {
    uint32_t binaryVersion;
    void *pBinary = nullptr;
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "CopyBuffer_simd8_", ".bin");

    size_t binarySize = loadDataFromFile(filePath.c_str(), pBinary);
    cl_int retVal = program->processElfBinary(pBinary, binarySize, binaryVersion);

    const auto &options = program->getOptions();
    size_t optionsSize = strlen(options.c_str()) + 1;
    auto pTmpGenBinary = new char[program->genBinarySize];
    auto pTmpIrBinary = new char[program->irBinarySize];
    auto pTmpOptions = new char[optionsSize];

    memcpy_s(pTmpGenBinary, program->genBinarySize, program->genBinary, program->genBinarySize);
    memcpy_s(pTmpIrBinary, program->irBinarySize, program->irBinary, program->irBinarySize);
    memcpy_s(pTmpOptions, optionsSize, options.c_str(), optionsSize);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, memcmp(pBinary, program->elfBinary.data(), binarySize));
    EXPECT_NE(0u, binaryVersion);

    // delete program's elf reference to force a resolve
    program->isProgramBinaryResolved = false;
    program->programBinaryType = GetParam();
    retVal = program->resolveProgramBinary();
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, memcmp(pTmpGenBinary, program->genBinary, program->genBinarySize));
    EXPECT_EQ(0, memcmp(pTmpIrBinary, program->irBinary, program->irBinarySize));
    EXPECT_EQ(0, memcmp(pTmpOptions, options.c_str(), optionsSize));

    delete[] pTmpGenBinary;
    delete[] pTmpIrBinary;
    delete[] pTmpOptions;

    deleteDataReadFromFile(pBinary);
}

INSTANTIATE_TEST_CASE_P(ResolveBinaryTests,
                        ProcessElfBinaryTestsWithBinaryType,
                        ::testing::ValuesIn(BinaryTypeValues));

TEST_F(ProcessElfBinaryTests, BackToBack) {
    uint32_t binaryVersion;
    void *pBinary = nullptr;
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "CopyBuffer_simd8_", ".bin");

    size_t binarySize = loadDataFromFile(filePath.c_str(), pBinary);
    cl_int retVal = program->processElfBinary(pBinary, binarySize, binaryVersion);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, memcmp(pBinary, program->elfBinary.data(), binarySize));
    EXPECT_NE(0u, binaryVersion);
    deleteDataReadFromFile(pBinary);

    std::string filePath2;
    retrieveBinaryKernelFilename(filePath2, "simple_arg_int_", ".bin");

    binarySize = loadDataFromFile(filePath2.c_str(), pBinary);
    retVal = program->processElfBinary(pBinary, binarySize, binaryVersion);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, memcmp(pBinary, program->elfBinary.data(), binarySize));
    EXPECT_NE(0u, binaryVersion);
    deleteDataReadFromFile(pBinary);
}

TEST_F(ProcessElfBinaryTests, BuildOptionsEmpty) {
    uint32_t binaryVersion;
    void *pBinary = nullptr;
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "simple_kernels_", ".bin");

    size_t binarySize = loadDataFromFile(filePath.c_str(), pBinary);
    cl_int retVal = program->processElfBinary(pBinary, binarySize, binaryVersion);

    EXPECT_EQ(CL_SUCCESS, retVal);
    const auto &options = program->getOptions();
    size_t optionsSize = strlen(options.c_str()) + 1;
    EXPECT_EQ(0, memcmp("", options.c_str(), optionsSize));
    EXPECT_NE(0u, binaryVersion);
    deleteDataReadFromFile(pBinary);
}

TEST_F(ProcessElfBinaryTests, BuildOptionsNotEmpty) {
    uint32_t binaryVersion;
    void *pBinary = nullptr;
    std::string filePath;
    retrieveBinaryKernelFilename(filePath, "simple_kernels_opts_", ".bin");

    size_t binarySize = loadDataFromFile(filePath.c_str(), pBinary);
    cl_int retVal = program->processElfBinary(pBinary, binarySize, binaryVersion);

    EXPECT_EQ(CL_SUCCESS, retVal);
    const auto &options = program->getOptions();
    size_t optionsSize = strlen(options.c_str()) + 1;
    std::string buildOptionsNotEmpty = "-cl-opt-disable -DDEF_WAS_SPECIFIED=1";
    EXPECT_EQ(0, memcmp(buildOptionsNotEmpty.c_str(), options.c_str(), optionsSize));
    EXPECT_NE(0u, binaryVersion);
    deleteDataReadFromFile(pBinary);
}
