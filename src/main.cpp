extern "C"
    {
#include "LDSP.h"
#include "LSP.h"
#include "LGraph.h"
#include "LGraphGen.h"
#include "LEdgeInfo.h"
#include "LString.h"
#include "LArray.h"
#include "LDebug.h"
#include "LFile.h"
#include "LException.h"
#include "LMemory.h"
#include "LTime.h"
#include "LRandSource.h"
#include "LFile.h"

#include "CAPSP_C.h"
#include "CDAPSP_DE.h"
#include "CDAPSP.h"
#include "CDAPSP_D.h"
    }

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "utils.h"
#include "floyd.h"
#include "graphGen.h"


// Make Graph type first, bcs params depend on GType
static char const* s_usageMsg =
"Usage: AType GType TestCount [i]UpdateCount GarphParams\n"\
"\nParams:\n"\
"AType \t\t- Algorithm: S_DIJ, S_FLOYD, S_LSP, D_DI, D_RR\n"\
"GType \t\t- Graph: RND, RND_C, CLUSTER, POWER, REAL\n"\
"[i]UpdateCount \t- if count is prefixed with 'i' ins/del update sequence will be generated\n"\
"\nGraphParams:\n"\
"RND, RND_C, POWER \t- NodeCount, EdgePStart, EdgePEnd, EdgePStep\n"\
"CLUSTER \t\t- NodeCount, ClusterCount, InterClusterP, EdgePStart, EdgePEnd, EdgePStep\n"\
"REAL \t\t\t- Filename\n";

struct _TUpdate
    {
    LGraph_TEdge* mEdge;
    ui4 mWeight;
    };

enum AlgorithmType
    {
    ALG_S_DIJ,
    ALG_S_FLOYD,
    ALG_S_LSP,
    ALG_D_DI,
    ALG_D_RR,
    ALG_UNKNOWN
    };

enum GraphType
    {
    GRAPH_RANDOM,
    GRAPH_RANDOM_CYCLE,
    GRAPH_CLUSTER,
    GRAPH_POWER,
    GRAPH_REAL,
    GRAPH_UNKNOWN
    };

LGraph* s_graph;
LEdgeInfo* s_edgeWeights;
LArray* s_updateSequence;

static AlgorithmType s_algorithmType = ALG_S_DIJ;
static GraphType s_graphType = GRAPH_RANDOM;
int s_graphCount = 0;
int s_nodeCount = 0;
int s_edgesPerNode = 10;
int s_clusterCount = 10;
float s_clusterPInter = 0.1f;
float s_edgeP = 0.0f;
static int s_updateCount = 0;
static bool s_insDelUpdates = false;
static float s_edgePRangeStart = 0.0f;
static float s_edgePRangeEnd = 0.0f;
static float s_edgePStep = 1.0f;
std::string s_realFilename;
static bool s_algIsDynamic = false;

static Timer s_timer;
static Timer s_initTimer;

static bool parseCmdLineArguments (int argc, char** argv);
static bool runTests();
static bool generateGraph (int seed);
static bool generateUpdateSequence (int seed);
static void executeAlgorithm();
static std::string getOutputFilename();

int main (int argc, char** argv)
    {
    if (argc == 1)
        {
        printf (s_usageMsg);
        return 0;
        }

    if (!parseCmdLineArguments (argc, argv))
        {
        printf ("Failed to parse command line arguments.\n");
        return 1;
        }

    setvbuf (stdout, NULL, _IONBF, 0);
    if (!runTests())
        {
        printf ("Failed to run tests.\n");
        return 1;
        }

    return 0;
    }

static void initFileStream (FILE** pFile, char const* pPrefixName)
    {
    char const* pDirName = "./results/";
    char const* pExtension = ".txt";

    std::string filename (pDirName);
    if (pPrefixName != nullptr)
        filename += pPrefixName;
    filename += getOutputFilename().c_str();
    filename += pExtension;

    *pFile = fopen (filename.c_str(), "w");
    if (*pFile == nullptr)
        Throw (LFile_CANT_OPEN_FILE);
    }

static bool runTests_real();

static bool runTests()
    {
    if (s_graphType == GRAPH_REAL)
        return runTests_real();

    FILE *pOutputFile, *pDeltaFile, *pInitFile;
    initFileStream (&pOutputFile, nullptr);
    initFileStream (&pDeltaFile, "_delta_");
    if (s_algIsDynamic)
        initFileStream (&pInitFile, "_init_");

    for (s_edgeP = s_edgePRangeStart; s_edgeP < s_edgePRangeEnd + 0.0001; s_edgeP += s_edgePStep)
        {
        s_timer.Clear();
        s_initTimer.Clear();

        fprintf (pDeltaFile, "===== P: %.3f \r\n", s_edgeP);
        printf ("\n# P = %.3f\n", s_edgeP);
        for (int iTest = 0; iTest < s_graphCount; ++iTest)
            {
            int const seed = iTest + 1;
            if (!generateGraph (seed) || s_graph == nullptr)
                {
                printf ("Failed to generate graph.\n");
                return false;
                }
            if (!generateUpdateSequence (seed) || s_updateSequence == nullptr)
                {
                printf ("Failed to genereate update sequence.\n");
                return false;
                }

            executeAlgorithm();
            fprintf (pDeltaFile, "%f\r\n", s_timer.GetDelta() / (double)s_updateCount);
            if (iTest % 10 == 0) printf ("%d\n", iTest);
            else                 printf (".");
            }

        double time = s_timer.GetTotal() / (double)s_graphCount / (double)s_updateCount;
        fprintf (pOutputFile, "%f\r\n", time);
        if (s_algIsDynamic)
            fprintf (pInitFile, "%f\r\n", s_initTimer.GetTotal() / (double)s_graphCount);
        }

    fclose (pOutputFile);
    fclose (pDeltaFile);
    if (s_algIsDynamic)
        fclose (pInitFile);
    return true;
    }

static bool runTests_real()
    {
    FILE *pOutputFile, *pDeltaFile, *pInitFile;
    initFileStream (&pOutputFile, nullptr);
    initFileStream (&pDeltaFile, "_delta_");
    if (s_algIsDynamic)
        initFileStream (&pInitFile, "_init_");

    if (!generateGraph (1) || s_graph == nullptr)
        {
        printf ("Failed to generate graph.\n");
        return false;
        }
    fprintf (pOutputFile, "n=%d m=%d\r\n", s_graph->mNodesCount, s_graph->mEdgesCount);

    for (int iTest = 0; iTest < s_graphCount; ++iTest)
        {
        LEdgeInfo* pEdgeWeightsCopy = LGraphGen_CopyEdgeInfo (s_graph, s_edgeWeights);

        int const seed = iTest + 1;
        if (!generateUpdateSequence (seed) || s_updateSequence == nullptr)
            {
            printf ("Failed to genereate update sequence.\n");
            return false;
            }
        executeAlgorithm();
        fprintf (pDeltaFile, "%f\r\n", s_timer.GetDelta() / (double)s_updateCount);
        if (iTest % 10 == 0) printf ("%d\n", iTest);
        else                 printf (".");

        LEdgeInfo_Delete (&s_edgeWeights);
        s_edgeWeights = pEdgeWeightsCopy;
        }

    double time = s_timer.GetTotal() / (double)s_graphCount / (double)s_updateCount;
    fprintf (pOutputFile, "%f\r\n", time);
    if (s_algIsDynamic)
        fprintf (pInitFile, "%f\r\n", s_initTimer.GetTotal() / (double)s_graphCount);

    fclose (pOutputFile);
    fclose (pDeltaFile);
    if (s_algIsDynamic)
        fclose (pInitFile);
    return true;
    }

// --------------------------------------------------
// ARGUMENT PARSING
// --------------------------------------------------
static AlgorithmType parseAlgorithmType (char* pStr);
static GraphType parseGraphType (char* pStr);
static bool parseGraphParams_random (int argc, char** argv);
static bool parseGraphParams_cluster (int argc, char** argv);
static bool parseGraphParams_power (int argc, char** argv);
static bool parseGraphParams_real (int argc, char** argv);

static bool parseCmdLineArguments (int argc, char** argv)
    {
    int const defaultParamCount = 5;
    if (argc < defaultParamCount)
        return false;

    int iParam = 1;
    s_algorithmType = parseAlgorithmType (argv[iParam++]);
    if (ALG_UNKNOWN == s_algorithmType)
        return false;
    s_algIsDynamic = (s_algorithmType == ALG_D_DI) || (s_algorithmType == ALG_D_RR);

    s_graphType = parseGraphType (argv[iParam++]);
    if (GRAPH_UNKNOWN ==  s_graphType)
        return false;

    s_graphCount = atoi (argv[iParam++]);
    if (0 == s_graphCount)
        return false;

    if (*(argv[iParam]) == 'i')
        {
        s_insDelUpdates = true;
        (argv[iParam])++;
        }
    s_updateCount = atoi (argv[iParam++]);
    if (0 == s_updateCount)
        return false;

    argc += iParam;
    argv += iParam;
    switch (s_graphType)
        {
        case GRAPH_RANDOM:
        case GRAPH_RANDOM_CYCLE:
        case GRAPH_POWER:
            return parseGraphParams_random (argc, argv);
        case GRAPH_CLUSTER:
            return parseGraphParams_cluster (argc, argv);
        case GRAPH_REAL:
            return parseGraphParams_real (argc, argv);
        default:
            return false;
        }
    }

static AlgorithmType parseAlgorithmType (char* pStr)
    {
    if (0 == strcmp (pStr, "S_DIJ"))
        return ALG_S_DIJ;
    if (0 == strcmp (pStr, "S_FLOYD"))
        return ALG_S_FLOYD;
    if (0 == strcmp (pStr, "S_LSP"))
        return ALG_S_LSP;
    if (0 == strcmp (pStr, "D_DI"))
        return ALG_D_DI;
    if (0 == strcmp (pStr, "D_RR"))
        return ALG_D_RR;
    return ALG_UNKNOWN;
    }

static GraphType parseGraphType (char* pStr)
    {
    if (0 == strcmp (pStr, "RND"))
        return GRAPH_RANDOM;
    if (0 == strcmp (pStr, "RND_C"))
        return GRAPH_RANDOM_CYCLE;
    if (0 == strcmp (pStr, "CLUSTER"))
        return GRAPH_CLUSTER;
    if (0 == strcmp (pStr, "POWER"))
        return GRAPH_POWER;
    if (0 == strcmp (pStr, "REAL"))
        return GRAPH_REAL;
    return GRAPH_UNKNOWN;
    }

static bool parseGraphParams_random (int argc, char** argv)
    {
    if (argc < 4)
        return false;
    int iParam = 0;
    s_nodeCount = atoi (argv[iParam++]);
    if (0 == s_nodeCount)
        return false;
    s_edgePRangeStart = atof (argv[iParam++]);
    if (0.0 == s_edgePRangeStart)
        return false;
    s_edgePRangeEnd = atof (argv[iParam++]);
    if (0.0 == s_edgePRangeEnd)
        return false;
    s_edgePStep = atof (argv[iParam++]);
    if (0.0 == s_edgePStep)
        return false;
    s_edgeP = s_edgePRangeStart;
    return true;
    }

static bool parseGraphParams_cluster (int argc, char** argv)
    {
    if (argc < 6)
        return false;
    int iParam = 0;
    s_nodeCount = atoi (argv[iParam++]);
    if (0 == s_nodeCount)
        return false;
    s_clusterCount = atoi (argv[iParam++]);
    if (0 == s_clusterCount)
        return false;
    s_clusterPInter = atof (argv[iParam++]);
    if (0.0 == s_clusterPInter)
        return false;
    s_edgePRangeStart = atof (argv[iParam++]);
    if (0.0 == s_edgePRangeStart)
        return false;
    s_edgePRangeEnd = atof (argv[iParam++]);
    if (0.0 == s_edgePRangeEnd)
        return false;
    s_edgePStep = atof (argv[iParam++]);
    if (0.0 == s_edgePStep)
        return false;
    s_edgeP = s_edgePRangeStart;
    return true;
    }

static bool parseGraphParams_real (int argc, char** argv)
    {
    if (argc < 1)
        return false;
    s_realFilename = argv[0];
    return true;
    }

// --------------------------------------------------
// GRAPH GENERATION
// --------------------------------------------------
static bool generateGraph (int seed)
    {
    if (s_graph != nullptr)
        LGraph_Delete (&s_graph);
    if (s_edgeWeights != nullptr)
        LEdgeInfo_Delete (&s_edgeWeights);

    switch (s_graphType)
        {
        case GRAPH_RANDOM: generateGraph_random (seed); break;
        case GRAPH_RANDOM_CYCLE: generateGraph_randomCycle (seed); break;
        case GRAPH_CLUSTER: generateGraph_cluster (seed); break;
        case GRAPH_POWER: generateGraph_power (seed); break;
        case GRAPH_REAL: generateGraph_real(); break;
        default:
            return false;
        }
    return true;
    }

// --------------------------------------------------
// UPDATE SEQUENCE GENERATION
// --------------------------------------------------
static void generateUpdateSequence_update (ui4 inSeed, ui4 inLength, ui4 inMinW, ui4 inMaxW);
static void generateUpdateSequence_insDel (ui4 inSeed, ui4 inLength);

static bool generateUpdateSequence (int seed)
    {
    if (s_updateSequence != nullptr)
        LArray_Delete (&s_updateSequence);

    if (s_graphType == GRAPH_REAL)
        generateUpdateSequence_insDel (seed, s_updateCount);
    else
        generateUpdateSequence_update (seed, s_updateCount, EDGE_WEIGHT_MIN, EDGE_WEIGHT_MAX);
    return true;
    }

static void generateUpdateSequence_update (ui4 inSeed, ui4 inLength, ui4 inMinW, ui4 inMaxW) {
    LRandSource* theSource       = NULL;
    LArray*      theEdgeList     = NULL;
    LEdgeInfo*   theEdgeInfoCopy = NULL;
    f8           theUpdType;

    if (s_updateSequence != NULL)
        LArray_Delete(&s_updateSequence);
    s_updateSequence = LArray_New(sizeof(_TUpdate));

    /* save initial edge weights */
    theEdgeInfoCopy = LGraphGen_CopyEdgeInfo (s_graph, s_edgeWeights);

    Try
        {
        _TUpdate theUpdate;
        ui4      theEdgeIdx, theWeight, i;

        theSource   = LRandSource_New((i4)inSeed);
        theEdgeList = LGraph_GetAllEdges(s_graph);

        for (i=0; i<inLength; ++i)
            {
            theUpdType = LRandSource_GetRandF8(theSource);
            theEdgeIdx = LRandSource_GetRandUI4(theSource, 0, LArray_GetItemsCount(theEdgeList)-1);
            theUpdate.mEdge = *(LGraph_TEdge**)LArray_ItemAt(theEdgeList, theEdgeIdx);
            theWeight = LEdgeInfo_UI4At(s_edgeWeights, theUpdate.mEdge);

            if (theWeight > inMaxW)
                theWeight = inMaxW;
            if (theWeight < inMinW)
                theWeight = inMinW;
            if (theUpdType < 0.5)
                theWeight = LRandSource_GetRandUI4(theSource, theWeight, inMaxW);
            else
                theWeight = LRandSource_GetRandUI4(theSource, inMinW, theWeight);

            theUpdate.mWeight = theWeight;
            LEdgeInfo_UI4At(s_edgeWeights, theUpdate.mEdge) = theWeight;

            if (theUpdate.mEdge->mSource->mIndex != theUpdate.mEdge->mTarget->mIndex)
                LArray_AppendItem(s_updateSequence, (void*)&theUpdate);
            else
                ++inLength;
            }
        }
    CatchAny
        {
        if (s_updateSequence!=NULL)
            LArray_Delete(&s_updateSequence);
        }

    if (theEdgeList!=NULL)
        LArray_Delete(&theEdgeList);
    if (theSource!=NULL)
        LRandSource_Delete(&theSource);

    /* restore initial edge weights */
    LEdgeInfo_Delete(&s_edgeWeights);
    s_edgeWeights = theEdgeInfoCopy;
    }

static void generateUpdateSequence_insDel (ui4 inSeed, ui4 inLength) {
    LRandSource* theSource   = NULL;
    LArray*      theEdgeList = NULL;

    if (s_updateSequence!=NULL) LArray_Delete(&s_updateSequence);
    s_updateSequence = LArray_New(sizeof(_TUpdate));

    Try
       {
        _TUpdate theUpdate;
        ui4      theEdgeIdx, theWeight, i;

        theSource   = LRandSource_New((i4)inSeed);
        theEdgeList = LGraph_GetAllEdges(s_graph);

        for (i=0; i<inLength; ++i)
            {
            f8 theProb = LRandSource_GetRandF8(theSource);

            theEdgeIdx = LRandSource_GetRandUI4(theSource, 0, LArray_GetItemsCount(theEdgeList)-1);
            if (theProb <= 0.5f)
                theWeight = EDGE_WIEGHT_INFINITY;
            else
                theWeight = 1;
            theUpdate.mEdge = *(LGraph_TEdge**)LArray_ItemAt(theEdgeList, theEdgeIdx);
            theUpdate.mWeight = theWeight;
            if (theUpdate.mEdge->mSource->mIndex != theUpdate.mEdge->mTarget->mIndex)
                LArray_AppendItem(s_updateSequence, (void*)&theUpdate);
            else
                ++inLength;
            }
        }
    CatchAny
        {
        if (s_updateSequence!=NULL)
            LArray_Delete(&s_updateSequence);
        LDebug_Print("** Couldn't generate sequence.\n");
        }
    if (theEdgeList!=NULL) LArray_Delete(&theEdgeList);
    if (theSource!=NULL)   LRandSource_Delete(&theSource);
}


// --------------------------------------------------
// ALGORITHM EXECUTION
// --------------------------------------------------
static void* static_dij() { return CAPSP_C_UI4 (s_graph, s_edgeWeights); }
static void* static_floyd() { return APSP_Floyd (s_graph, s_edgeWeights); }
static void* static_lsp() { return LSP_New (s_graph, s_edgeWeights); }
static void exectue_static (void* (*pAlgorithm)());

static void* dynamic_di_init() { return LDSP_New (s_graph, s_edgeWeights); }
static void dynamic_di_delete (void** ppStruct) { LDSP_Delete ((LDSP**)ppStruct); }
static void dynamic_di_update (void* pStruct, _TUpdate* pUpdate)
    {
    ui2 iSource = (ui2)pUpdate->mEdge->mSource->mIndex;
    ui2 iTarget = (ui2)pUpdate->mEdge->mTarget->mIndex;
    LDSP_UpdateEdge ((LDSP*)pStruct, iSource,iTarget, pUpdate->mWeight);
    }
static void* dynamic_rr_init() { return CDAPSP_DE_New (s_graph, s_edgeWeights); }
static void dynamic_rr_delete (void** ppStruct) { CDAPSP_DE_Delete ((CDAPSP_DE**)ppStruct); }
static void dynamic_rr_update (void* pStruct, _TUpdate* pUpdate)
    {
    CDAPSP_DE_UpdateEdge ((CDAPSP_DE*)pStruct, pUpdate->mEdge, pUpdate->mWeight);
    }
static void execute_dynamic (void* (*pInit)(), void (*pDelete)(void**), void (*pUpdate)(void*, _TUpdate*));

static void executeAlgorithm()
    {
    switch (s_algorithmType)
        {
        case ALG_S_DIJ:
            return exectue_static (static_dij);
        case ALG_S_FLOYD:
            return exectue_static (static_floyd);
        case ALG_S_LSP:
            return exectue_static (static_lsp);
        case ALG_D_DI:
            return execute_dynamic (dynamic_di_init, dynamic_di_delete, dynamic_di_update);
        case ALG_D_RR:
            return execute_dynamic (dynamic_rr_init, dynamic_rr_delete, dynamic_rr_update);
        default:
            break;
        }
    }

static void exectue_static (void* (*pAlgorithm)())
    {
    if (pAlgorithm == nullptr)
        return;

    LException* theException;
    Try
        {
        s_timer.Start();
        ui4 seqLength = LArray_GetItemsCount (s_updateSequence);
        for (ui4 i = 0; i < seqLength; ++i)
            {
            _TUpdate update = *(_TUpdate*)LArray_ItemAt(s_updateSequence, i);
            LEdgeInfo_UI4At (s_edgeWeights, update.mEdge) = update.mWeight;

            void* pResult = pAlgorithm();
            if (s_algorithmType == ALG_S_LSP)
                LSP_Delete ((LSP**)&pResult);
            else
                LMemory_Free (&pResult);
            }
        s_timer.Stop();
        }
    Catch (theException)
        {
        debug_printf ("Couldn't run algorithm.\n");
        LException_Dump (theException);
        }
    }

static void execute_dynamic (void* (*pInit)(), void (*pDelete)(void**), void (*pUpdate)(void*, _TUpdate*))
    {
    if (pInit == nullptr || pDelete == nullptr || pUpdate == nullptr)
        return;

    LException* theException;
    Try
        {
        s_initTimer.Start();
        void* pStruct = pInit();
        s_initTimer.Stop();

        s_timer.Start();
        ui4 seqLength = LArray_GetItemsCount (s_updateSequence);
        for (ui4 i = 0; i < seqLength; ++i)
            {
            _TUpdate update = *(_TUpdate*)LArray_ItemAt(s_updateSequence, i);
            pUpdate (pStruct, &update);
            }
        s_timer.Stop();
        pDelete (&pStruct);
        }
    Catch (theException)
        {
        debug_printf ("Couldn't run algorithm.\n");
        LException_Dump (theException);
        }
    }

static std::string getOutputFilename()
    {
    char const* pAlgName = "unknown";
    switch (s_algorithmType)
        {
        case ALG_S_DIJ: pAlgName = "s_dij"; break;
        case ALG_S_FLOYD: pAlgName = "s_floyd"; break;
        case ALG_S_LSP: pAlgName = "s_lsp"; break;
        case ALG_D_DI: pAlgName = "d_di"; break;
        case ALG_D_RR: pAlgName = "d_rr"; break;
        }

    char const* pGraphName = "unknown";
    switch (s_graphType)
        {
        case GRAPH_RANDOM: pGraphName = "rnd"; break;
        case GRAPH_RANDOM_CYCLE: pGraphName = "rndC"; break;
        case GRAPH_CLUSTER: pGraphName = "cluster"; break;
        case GRAPH_POWER: pGraphName = "power"; break;
        case GRAPH_REAL: pGraphName = "real"; break;
        }

    char buffer[256] = {0};
    switch (s_graphType)
        {
        case GRAPH_RANDOM:
        case GRAPH_RANDOM_CYCLE:
        case GRAPH_POWER:
            sprintf (buffer, "%s_%s_%d_%d_%.3f_%.3f", pAlgName, pGraphName, s_updateCount,
                     s_nodeCount, s_edgePRangeStart, s_edgePRangeEnd);
            break;
        case GRAPH_CLUSTER:
            sprintf (buffer, "%s_%s_%d_%d_%.3f_%.3f_%.3f", pAlgName, pGraphName, s_updateCount,
                     s_nodeCount, s_clusterPInter, s_edgePRangeStart, s_edgePRangeEnd);
            break;
        case GRAPH_REAL:
            sprintf (buffer, "%s_%s_%d_%d", pAlgName, pGraphName, s_graphCount, s_updateCount);
            break;
        default:
            sprintf (buffer, "unknown");
        }
    return std::string (buffer);
    }
