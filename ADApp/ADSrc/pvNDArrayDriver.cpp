#include <pv/pvPortDriver.h>

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <epicsString.h>
#include <epicsStdio.h>

#define epicsExportSharedSymbols
#include <shareLib.h>
#include "NDArrayPool.h"
#include "ADCoreVersion.h"
#include "tinyxml.h"
//#include "PVAttribute.h"
//#include "paramAttribute.h"
//#include "functAttribute.h"
#include "pvNDArrayDriver.h"

#define MAX_PATH_PARTS 32

#if defined(_WIN32)              // Windows
  #include <direct.h>
  #define strtok_r(a,b,c) strtok(a,b)
  #define MKDIR(a,b) _mkdir(a)
  #define delim "\\"
#elif defined(vxWorks)           // VxWorks
  #include <sys/stat.h>
  #define MKDIR(a,b) mkdir(a)
  #define delim "/"
#else                            // Linux
  #include <sys/stat.h>
  #include <sys/types.h>
  #define delim "/"
  #define MKDIR(a,b) mkdir(a,b)
#endif

using namespace std;
using namespace epics::nt;
using namespace epics::pvData;
using namespace epics::pvDatabase;
using namespace epics::pvPortDriver;

PVNDArrayDriver::PVNDArrayDriver (std::string const & prefix, int maxBuffers, size_t maxMemory)
: PVPortDriver(prefix), mNDArrayPool(new NDArrayPool(maxBuffers, maxMemory))/*,
  mAttributeList(new NDAttributeList())*/
{
    char versionString[20];
    epicsSnprintf(versionString, sizeof(versionString), "%d.%d.%d",
                      ADCORE_VERSION, ADCORE_REVISION, ADCORE_MODIFICATION);

    NDPortNameSelf          = createParam<string>(NDPortNameSelfString, prefix);
    NDADCoreVersion         = createParam<string>(NDADCoreVersionString, versionString);
    NDArraySizeX            = createParam<int32>(NDArraySizeXString, 0);
    NDArraySizeY            = createParam<int32>(NDArraySizeYString, 0);
    NDArraySizeZ            = createParam<int32>(NDArraySizeZString, 0);
    NDArraySize             = createParam<int32>(NDArraySizeString, 0);
    NDNDimensions           = createParam<int32>(NDNDimensionsString, 0);
    NDDimensions            = createParam<int32>(NDDimensionsString);
    NDDataType              = createParam<int32>(NDDataTypeString, pvUByte);
    NDColorMode             = createParam<int32>(NDColorModeString, NDColorModeMono);
    NDUniqueId              = createParam<int32>(NDUniqueIdString, 0);
    NDTimeStamp             = createParam<double>(NDTimeStampString, 0.0);
    NDEpicsTSSec            = createParam<int32>(NDEpicsTSSecString, 0);
    NDEpicsTSNsec           = createParam<int32>(NDEpicsTSNsecString, 0);
    NDBayerPattern          = createParam<int32>(NDBayerPatternString, 0);
    NDArrayCounter          = createParam<int32>(NDArrayCounterString, 0);
    NDFilePath              = createParam<string>(NDFilePathString, "");
    NDFilePathExists        = createParam<boolean>(NDFilePathExistsString);
    NDFileName              = createParam<string>(NDFileNameString, "");
    NDFileNumber            = createParam<int32>(NDFileNumberString, 0);
    NDFileTemplate          = createParam<string>(NDFileTemplateString, "%s%s_%3.3d.dat");
    NDAutoIncrement         = createParam<boolean>(NDAutoIncrementString, false);
    NDFullFileName          = createParam<string>(NDFullFileNameString);
    NDFileFormat            = createParam<int32>(NDFileFormatString);
    NDAutoSave              = createParam<boolean>(NDAutoSaveString);
    NDWriteFile             = createParam<int32>(NDWriteFileString, 0);
    NDReadFile              = createParam<int32>(NDReadFileString, 0);
    NDFileWriteMode         = createParam<int32>(NDFileWriteModeString);
    NDFileWriteStatus       = createParam<int32>(NDFileWriteStatusString, 0);
    NDFileWriteMessage      = createParam<string>(NDFileWriteMessageString, "");
    NDFileNumCapture        = createParam<int32>(NDFileNumCaptureString);
    NDFileNumCaptured       = createParam<int32>(NDFileNumCapturedString, 0);
    NDFileCapture           = createParam<int32>(NDFileCaptureString, 0);
    NDFileDeleteDriverFile  = createParam<int32>(NDFileDeleteDriverFileString);
    NDFileLazyOpen          = createParam<int32>(NDFileLazyOpenString);
    NDFileCreateDir         = createParam<int32>(NDFileCreateDirString, 0);
    NDFileTempSuffix        = createParam<string>(NDFileTempSuffixString, "");
    NDAttributesFile        = createParam<string>(NDAttributesFileString);
    NDArrayData             = createNDArrayParam(NDArrayDataString);
    NDArrayCallbacks        = createParam<int32>(NDArrayCallbacksString);
    NDPoolMaxBuffers        = createParam<int32>(NDPoolMaxBuffersString, mNDArrayPool->maxBuffers());
    NDPoolAllocBuffers      = createParam<int32>(NDPoolAllocBuffersString, mNDArrayPool->numBuffers());
    NDPoolFreeBuffers       = createParam<int32>(NDPoolFreeBuffersString, mNDArrayPool->numFree());
    NDPoolMaxMemory         = createParam<double>(NDPoolMaxMemoryString);
    NDPoolUsedMemory        = createParam<double>(NDPoolUsedMemoryString);
}

/** Checks whether the directory specified in NDFilePath parameter exists.
  *
  * This is a convenience function that determines the directory specified in
  * NDFilePath parameter exists. It sets the value of NDFilePathExists to false
  * (does not exist) or true (exists). It also adds a trailing '/' character to
  * the path if one is not present.
  * Returns false if the directory does not exist.
  */
bool PVNDArrayDriver::checkPath (void)
{
    // Formats a complete file name from the components defined in
    // NDStdDriverParams
    char filePath[MAX_FILENAME_LEN];
    char lastChar;
    bool hasTerminator = false, isDir = false;
    struct stat buff;
    int istat;
    size_t len;
    bool pathExists = false;

    strncpy(filePath, NDFilePath->get().c_str(), sizeof(filePath));
    len = strlen(filePath);
    if (len == 0)
        return false;

    // If the path contains a trailing '/' or '\' remove it, because Windows
    // won't find the directory if it has that trailing character
    lastChar = filePath[len-1];
#ifdef _WIN32
    if ((lastChar == '/') || (lastChar == '\\'))
#else
    if (lastChar == '/')
#endif
    {
        filePath[len-1] = 0;
        len--;
        hasTerminator = true;
    }

    istat = stat(filePath, &buff);
    if (!istat)
        isDir = !!(S_IFDIR & buff.st_mode);

    pathExists = !istat && isDir;

    // If the path didn't have a trailing terminator then add it if there's room
    if (!hasTerminator)
    {
        if (len < MAX_FILENAME_LEN - 2)
            strcat(filePath, delim);
        NDFilePath->put(filePath);
    }
    NDFilePathExists->put(pathExists);
    return pathExists;
}

/** Function to create a directory path for a file.
  * \param[in] path  Path to create. The final part is the file name and is not
  *                     created.
  * \param[in] pathDepth  This determines how much of the path to assume exists before attempting
  *                     to create directories:
  *                     pathDepth = 0 create no directories
  *                     pathDepth = 1 create all directories needed (i.e. only assume root directory exists).
  *                     pathDepth = 2  Assume 1 directory below the root directory exists
  *                     pathDepth = -1 Assume all but one direcory exists
  *                     pathDepth = -2 Assume all but two directories exist.
  */
int PVNDArrayDriver::createFilePath (const char *path, int pathDepth)
{
    int result = 0;
    char *parts[MAX_PATH_PARTS];
    int num_parts;
    char directory[MAX_FILENAME_LEN];

    // Initialise the directory to create
    char nextDir[MAX_FILENAME_LEN];

    // Extract the next name from the directory
    char* saveptr;
    int i=0;

    // Check for trivial case.
    if (pathDepth == 0)
        return 0;

    // Check for Windows disk designator
    if (path[1] == ':')
    {
        nextDir[0] = path[0];
        nextDir[1] = ':';
        i+=2;
    }

    // Skip over any more delimiters
    while ((path[i] == '/' || path[i] == '\\') && i < MAX_FILENAME_LEN)
    {
        nextDir[i] = path[i];
        ++i;
    }
    nextDir[i] = 0;

    // Now, tokenise the path, first making a copy because strtok is destructive
    strcpy(directory, &path[i] );
    num_parts = 0;
    parts[num_parts] = strtok_r( directory, "\\/", &saveptr);

    while ( parts[num_parts] != NULL )
        parts[++num_parts] = strtok_r(NULL, "\\/", &saveptr);

    // Handle the case if the path depth is negative
    if (pathDepth < 0)
    {
        pathDepth = num_parts + pathDepth;
        if (pathDepth < 1)
            pathDepth = 1;
    }

    // Loop through parts creating directories
    for ( i = 0; i < num_parts && !result; i++ )
    {
        strcat(nextDir, parts[i]);
        if ( i >= pathDepth )
            if(MKDIR(nextDir, 0777) != 0 && errno != EEXIST)
                result = -1;
        strcat(nextDir, delim);
   }

    return result;
}

/** Build a file name from component parts.
  * \param[in] maxChars  The size of the fullFileName string.
  * \param[out] fullFileName The constructed file name including the file path.
  *
  * This is a convenience function that constructs a complete file name
  * from the NDFilePath, NDFileName, NDFileNumber, and
  * NDFileTemplate parameters. If NDAutoIncrement is true then it increments the
  * NDFileNumber after creating the file name.
  */
int PVNDArrayDriver::createFileName (int maxChars, char *fullFileName)
{
    // Formats a complete file name from the components defined in
    // NDStdDriverParams
    char filePath[MAX_FILENAME_LEN];
    char fileName[MAX_FILENAME_LEN];
    char fileTemplate[MAX_FILENAME_LEN];
    int fileNumber;
    bool autoIncrement;
    int len;

    checkPath();
    strncpy(filePath, NDFilePath->get().c_str(), sizeof(filePath));
    strncpy(fileTemplate, NDFileTemplate->get().c_str(), sizeof(fileTemplate));
    fileNumber = NDFileNumber->get();
    autoIncrement = NDAutoIncrement->get();

    len = epicsSnprintf(fullFileName, maxChars, fileTemplate, filePath,
            fileName, fileNumber);

    if (len < 0)
        return -1;

    if (autoIncrement)
        NDFileNumber->put(fileNumber+1);

    return 0;
}

/** Build a file name from component parts.
  * \param[in] maxChars  The size of the fullFileName string.
  * \param[out] filePath The file path.
  * \param[out] fileName The constructed file name without file file path.
  *
  * This is a convenience function that constructs a file path and file name
  * from the NDFilePath, NDFileName, NDFileNumber, and
  * NDFileTemplate parameters. If NDAutoIncrement is true then it increments the
  * NDFileNumber after creating the file name.
  */
int PVNDArrayDriver::createFileName (int maxChars, char *filePath, char *fileName)
{
    // Formats a complete file name from the components defined in
    // NDStdDriverParams
    char fileTemplate[MAX_FILENAME_LEN];
    char name[MAX_FILENAME_LEN];
    int fileNumber;
    int autoIncrement;
    int len;

    checkPath();
    strncpy(filePath, NDFilePath->get().c_str(), maxChars);
    strncpy(name, NDFileName->get().c_str(), sizeof(name));
    strncpy(fileTemplate, NDFileTemplate->get().c_str(), sizeof(fileTemplate));
    fileNumber = NDFileNumber->get();
    autoIncrement = NDAutoIncrement->get();

    len = epicsSnprintf(fileName, maxChars, fileTemplate, name, fileNumber);

    if (len < 0)
        return -1;

    if (autoIncrement)
        NDFileNumber->put(fileNumber+1);

    return 0;
}

/** Create this driver's NDAttributeList (pAttributeList) by reading an XML file
  * \param[in] fileName  The name of the XML file to read.
  *
  * This clears any existing attributes from this drivers' NDAttributeList and then creates a new list
  * based on the XML file.  These attributes can then be associated with an NDArray by calling asynNDArrayDriver::getAttributes()
  * passing it pNDArray->pAttributeList.
  *
  * The following simple example XML file illustrates the way that both PVAttribute and paramAttribute attributes are defined.
  * <pre>
  * <?xml version="1.0" standalone="no" ?>
  * \<Attributes>
  * \<Attribute name="AcquireTime"         type="EPICS_PV" source="13SIM1:cam1:AcquireTime"      dbrtype="DBR_NATIVE"  description="Camera acquire time"/>
  * \<Attribute name="CameraManufacturer"  type="PARAM"    source="MANUFACTURER"                 datatype="STRING"     description="Camera manufacturer"/>
  * \</Attributes>
  * </pre>
  * Each NDAttribute (currently either an PVAttribute or paramAttribute, but other types may be added in the future)
  * is defined with an XML <b>Attribute</b> tag.  For each attribute there are a number of XML attributes
  * (unfortunately there are 2 meanings of attribute here: the NDAttribute and the XML attribute).
  * XML attributes have the syntax name="value".  The XML attribute names are case-sensitive and must be lower case, i.e. name="xxx", not NAME="xxx".
  * The XML attribute values are specified by the XML Schema and are always uppercase for <b>datatype</b> and <b>dbrtype</b> attributes.
  * The XML attribute names are listed here:
  *
  * <b>name</b> determines the name of the NDAttribute.  It is required, must be unique, is case-insensitive,
  * and must start with a letter.  It can include only letters, numbers and underscore. (No whitespace or other punctuation.)
  *
  * <b>type</b> determines the type of the NDAttribute.  "EPICS_PV" creates a PVAttribute, while "PARAM" creates a paramAttribute.
  * The default is EPICS_PV if this XML attribute is absent.
  *
  * <b>source</b> determines the source of the NDAttribute.  It is required. If type="EPICS_PV" then this is the name of the EPICS PV, which is
  * case-sensitive. If type="PARAM" then this is the drvInfo string that is used in EPICS database files (e.g. ADBase.template) to identify
  * this parameter.
  *
  * <b>dbrtype</b> determines the data type that will be used to read an EPICS_PV value with channel access.  It can be one of the standard EPICS
  * DBR types (e.g. "DBR_DOUBLE", "DBR_STRING", ...) or it can be the special type "DBR_NATIVE" which means to use the native channel access
  * data type for this PV.  The default is DBR_NATIVE if this XML attribute is absent.  Always use uppercase.
  *
  * <b>datatype</b> determines the parameter data type for type="PARAM".  It must match the actual data type in the driver or plugin
  * parameter library, and must be "INT", "DOUBLE", or "STRING".  The default is "INT" if this XML attribute is absent.   Always use uppercase.
  *
  * <b>addr</b> determines the asyn addr (address) for type="PARAM".  The default is 0 if the XML attribute is absent.
  *
  * <b>description</b> determines the description for this attribute.  It is not required, and the default is a NULL string.
  *
  *
int PVNDArrayDriver::readNDAttributesFile(const char *fileName)
{
    const char *pName, *pSource, *pAttrType, *pDescription=NULL;
    TiXmlDocument doc(fileName);
    TiXmlElement *Attr, *Attrs;

    // Clear any existing attributes
    mAttributeList->clear();
    if (!fileName || (strlen(fileName) == 0)) return 0;

    if (!doc.LoadFile()) {
        //asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
        //    "%s:%s: cannot open file %s error=%s\n",
        //    driverName, functionName, fileName, doc.ErrorDesc());
        return -1;
    }

    Attrs = doc.FirstChildElement( "Attributes" );
    if (!Attrs) {
        //asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
        //    "%s:%s: cannot find Attributes element\n",
        //    driverName, functionName);
        return -1;
    }

    for (Attr = Attrs->FirstChildElement(); Attr; Attr = Attr->NextSiblingElement()) {
        pName = Attr->Attribute("name");
        if (!pName) {
            //asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            //    "%s:%s: name attribute not found\n",
            //    driverName, functionName);
            return -1;
        }

        pDescription = Attr->Attribute("description");
        pSource = Attr->Attribute("source");
        pAttrType = Attr->Attribute("type");
        if (!pAttrType) pAttrType = NDAttribute::attrSourceString(NDAttrSourceEPICSPV);
        if (strcmp(pAttrType, NDAttribute::attrSourceString(NDAttrSourceEPICSPV)) == 0) {
            const char *pDBRType = Attr->Attribute("dbrtype");
            int dbrType = DBR_NATIVE;
            if (pDBRType) {
                if      (!strcmp(pDBRType, "DBR_CHAR"))   dbrType = DBR_CHAR;
                else if (!strcmp(pDBRType, "DBR_SHORT"))  dbrType = DBR_SHORT;
                else if (!strcmp(pDBRType, "DBR_ENUM"))   dbrType = DBR_ENUM;
                else if (!strcmp(pDBRType, "DBR_INT"))    dbrType = DBR_INT;
                else if (!strcmp(pDBRType, "DBR_LONG"))   dbrType = DBR_LONG;
                else if (!strcmp(pDBRType, "DBR_FLOAT"))  dbrType = DBR_FLOAT;
                else if (!strcmp(pDBRType, "DBR_DOUBLE")) dbrType = DBR_DOUBLE;
                else if (!strcmp(pDBRType, "DBR_STRING")) dbrType = DBR_STRING;
                else if (!strcmp(pDBRType, "DBR_NATIVE")) dbrType = DBR_NATIVE;
                else {
                    //asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    //    "%s:%s: unknown dbrType = %s\n",
                    //    driverName, functionName, pDBRType);
                    return -1;
                }
            }
            //asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
            //    "%s:%s: Name=%s, PVName=%s, pDBRType=%s, dbrType=%d, pDescription=%s\n",
            //    driverName, functionName, pName, pSource, pDBRType, dbrType, pDescription);
#ifndef EPICS_LIBCOM_ONLY
            PVAttribute *pPVAttribute = new PVAttribute(pName, pDescription, pSource, dbrType);
            mAttributeList->add(pPVAttribute);
#endif
        } else if (strcmp(pAttrType, NDAttribute::attrSourceString(NDAttrSourceParam)) == 0) {
            const char *pDataType = Attr->Attribute("datatype");
            if (!pDataType) pDataType = "int";
            const char *pAddr = Attr->Attribute("addr");
            int addr=0;
            if (pAddr) addr = strtol(pAddr, NULL, 0);
            //asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
            //    "%s:%s: Name=%s, drvInfo=%s, dataType=%s,pDescription=%s\n",
            //    driverName, functionName, pName, pSource, pDataType, pDescription);

            PVRecordPtr record(findParam(pSource));
            if(!record)
                return -1;

            NTScalarPtr scalar(NTScalar::wrap(record->getPVRecordStructure()->getPVStructure()));
            if(!scalar)
                return -1;

            paramAttribute *pParamAttribute = new paramAttribute(pName, pDescription, pSource, addr, scalar, pDataType);
            mAttributeList->add(pParamAttribute);
        } else if (strcmp(pAttrType, NDAttribute::attrSourceString(NDAttrSourceFunct)) == 0) {
            const char *pParam = Attr->Attribute("param");
            if (!pParam) pParam = epicsStrDup("");
            //asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
            //    "%s:%s: Name=%s, function=%s, pParam=%s, pDescription=%s\n",
            //    driverName, functionName, pName, pSource, pParam, pDescription);
#ifndef EPICS_LIBCOM_ONLY
            functAttribute *pFunctAttribute = new functAttribute(pName, pDescription, pSource, pParam);
            mAttributeList->add(pFunctAttribute);
#endif
        }
    }
    // Wait a short while for channel access callbacks on EPICS PVs
    epicsThreadSleep(0.5);
    // Get the initial values
    mAttributeList->updateValues();
    return 0;
}

int PVNDArrayDriver::getAttributes(NDAttributeList *pList)
{
    int status = 0;
    status = mAttributeList->updateValues();
    status = mAttributeList->copy(pList);
    return status;
}
*/

/** Called when an underlying record is processed.
  * This function performs actions for some parameters, including NDAttributesFile.
  * \param[in] record The record being processed. */
void PVNDArrayDriver::process (PVRecord const * record)
{
    if(record == NDAttributesFile.get())
    {
        //readNDAtributesFile
    }
    else if(record == NDFilePath.get())
    {
        if(!checkPath())
        {
            int pathDepth = NDFileCreateDir->get();
            createFilePath(NDFilePath->get().c_str(), pathDepth);
            checkPath();
        }
    }
}

void PVNDArrayDriver::report (FILE *fp, int details)
{
    //PVPortDriver::report(fp, details);
}
