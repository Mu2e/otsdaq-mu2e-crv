/// OTS data handling utility functions ///

const LID_GATEWAY = 200
const LID_CONSOLE = 260
const LID_CONFIG = 281
const LID_SLOWCONTROLS = 282
const LID_MACROMAKER = 800
const LID_TFM = 302
const TFM_PORT = 3047

// Need to make this page specific
getAppStatusEnabled = true;
getCurrentStateEnabled = true;
getAliasListEnabled = true;
getAlarmChecksEnabled = true;
getSystemMessagesEnabled = true;
getArtdaqEnabled = true;

DCS_PREFIX = "Mu2e:TDAQ_crv"

CookieCode = "29F0C2E03E9543DD5893E20BD8F1AD16CA3B255C1AA9C1F93640E28121224F4A12112A50A76E2DFF01100BD901B8EFCBF314280DBDE907F429E9754A0BC4941DD6BE6D7D2C9A7C2DAA8706AB40F577330A9F41C78848BBB13130FB3CF58F59CB4DC6487A60C4A70B4CAEB68CA32DBFADCC007554483005796100B5568F0E21DDD46957352DFE4079ACF6055024C5FDF0C572450EA34A87044B3D5ADA4B7BB720E40E55110D958BB98B9009AF5507A01B79E5291C2FB0207AED7A5539F50C59D91BAEEB284376E1CE06EB7E5CF21E776B03A0883250A8AD3E230277180ED0F2297EDD51C153338F591E0DB5102B2C7B2ECC03611DAC0E5BCF10D2E71EA2D94820"



// Main XmlHttpRequest get call
function get(RequestType, data = "", lid = 200, type1 = "Request") {
    let base_url = window.location.origin;
    if (lid === LID_SLOWCONTROLS) {
        base_url = window.location.protocol + "//" +
            window.location.hostname + ":" +
            (parseInt(window.location.port, 10) + 1).toString()
    } else if (lid === LID_TFM) {
        base_url = window.location.protocol + "//" +
            window.location.hostname + ":" +
            (TFM_PORT).toString()
    }
    let url = base_url +
        '/urn:xdaq-application:lid=' +
        lid.toString();
    // Default reqeusts all go to /Request
    // overwrite here for special cases
    switch (type1) {
        case 'transition':
            url += '/StateMachineXgiHandler?StateMachine=' +
                RequestType;
            break;

        default:
            url += '/Request?RequestType=' +
                RequestType;
    }

    return fetch(url, { method: 'POST', body: data })
        .then(response => {
            if (!response.ok) {
                throw new Error(`Network response error: ${response.status}`);
            }
            return response.text();
        })
        .then(xmlString => {
            const parser = new DOMParser();
            const xmlDoc = parser.parseFromString(xmlString, "text/xml");
            return xmlDoc.querySelector('ROOT'); // Return the parsed XML document
        })
        .catch(error => {
            console.error('Error fetching status:', error);
            return undefined;
        });
}

// Parse an xml document recursively into an javascript object (json) that is much eaiser to handle down the line
// If the same attribute are present multiple time, they are converted to a list.
// If tryToNeastBy is not None (but for example "name"), the function tries to generat an object neasted by that attribute
function xmlToJson(xmlNode) {
    if (xmlNode.nodeType === Node.ELEMENT_NODE) {
        const obj = {};
        // Handle attributes
        if (xmlNode.hasAttributes()) {
            for (let i = 0; i < xmlNode.attributes.length; i++) {
                const attr = xmlNode.attributes[i];
                obj[attr.name] = attr.value;
            }
        }

        // Handle children
        if (xmlNode.hasChildNodes()) {
            for (let i = 0; i < xmlNode.childNodes.length; i++) {
                const childNode = xmlNode.childNodes[i];

                // Ignore text nodes and comments
                if (childNode.nodeType === Node.ELEMENT_NODE) {
                    childObj = xmlToJson(childNode);
                    if (childObj.hasOwnProperty('value') &&
                        ((childNode.nodeName != "messages") //&&             // exceptions to keep value for messages
                        )) {
                        if (childObj.value === '') {
                            if (childNode.childNodes.length == 0) {
                                childObj = " "
                            }
                        } else {
                            if (Object.keys(childObj).length > 1) {
                                name_ = childObj.value;
                                delete childObj.value;
                                let newChildObj = {}
                                newChildObj[name_] = childObj
                                childObj = newChildObj;
                            } else {
                                childObj = childObj.value
                            }
                        }
                    }
                    const childName = childNode.nodeName;
                    //console.log("DEBUG", childName, childObj)

                    // Handle multiple children with the same name
                    if (obj[childName]) {
                        // Already exists, convert to array
                        if (!Array.isArray(obj[childName])) {
                            obj[childName] = [obj[childName]];
                        }
                        obj[childName].push(childObj);
                    } else {
                        obj[childName] = childObj;
                    }
                }
            }

        }
        return obj;
    }
}

// Some of the OTS xml data is not neasted, neasting them will make data handling easier downstream
function neastJson(oldObj, attribute = "name", group = null) {
    if (oldObj.hasOwnProperty(attribute)) {
        let newObj = {}
        if (group) {
            newObj[group] = {}
            ptObj = newObj[group];
        } else {
            ptObj = newObj;
        }
        for (let i = 0; i < oldObj[attribute].length; i++) {
            const name = oldObj[attribute][i]
            ptObj[name] = {}
            //loop over all other attributes
            for (const property in oldObj) {
                if (property !== attribute) {
                    if (Array.isArray(oldObj[property]) && Array.isArray(oldObj[attribute]) &&
                        oldObj[property].length === oldObj[attribute].length) {
                        ptObj[name][property] = oldObj[property][i];
                    } else {
                        newObj[property] = oldObj[property]
                    }
                }
            }
        }
        return newObj;
    } else {
        return oldObj;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////// Some utility functions //////////////////////////////////
function formatTime(seconds, noHours = false) { // from gemini
    const date = new Date(seconds * 1000);
    const hours = date.getUTCHours();
    const minutes = date.getUTCMinutes();
    const secs = date.getSeconds();
    const formattedHours = hours.toString().padStart(2, '0');
    const formattedMinutes = minutes.toString().padStart(2, '0');
    const formattedSeconds = secs.toString().padStart(2, '0');
    if (noHours) return `${formattedMinutes}:${formattedSeconds}`;
    else return `${formattedHours}:${formattedMinutes}:${formattedSeconds}`;
}

function addTime(div, time, dtime = 0) {
    let span = div.querySelector("span")
    if (span == undefined) {
        span = document.createElement("span")
        span.classList.add("mu2e_right_float");
        span.classList.add("mu2e_dcs_timestamp");
        div.appendChild(span)
    }
    if (dtime > 120) {
        setStatusColor(span, "mu2e_bad_text")
    }
    span.innerHTML = time.toLocaleTimeString(['en-GB'], { hour: '2-digit', minute: '2-digit', second: '2-digit' })
    //span.innerHTML = formatTime(dtime)
}

///////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// OTS API - OTS communciation functions //////////////////////////

async function getAppStatus() {
    try {
        const xml = await get('getAppStatus', lid = LID_GATEWAY);
        return neastJson(xmlToJson(xml).DATA, "name");
    } catch (error) { console.error("Error:", error); }
}

async function restartContext(context) {
    try {
        const xml = await get("restartApps&contextName=" + context, lid = LID_GATEWAY);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

async function getAliasList() {
    try {
        const xml = await get('getAliasList', lid = LID_GATEWAY);
        //console.log(xmlToJson(xml).DATA)
        return neastJson(xmlToJson(xml).DATA, "config_alias", group = "aliases");
    } catch (error) { console.error("Error:", error); }
}


// Returns all contexts, no inforamtion about active or not
async function getContextNames() {
    try {
        const xml = await get('getContextNames', lid = LID_GATEWAY);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

async function getSystemMessages(history = false) {
    try {
        data = history ? "history=true" : "";
        const xml = await get('getSystemMessages',
            data = data, lid = LID_GATEWAY);
        let json = xmlToJson(xml).DATA
        let parts = decodeURIComponent(json["systemMessages"]).split("|");
        out = []
        if (parts.length > 1) {
            for (let i = 0; i < parts.length / 2; i++) {
                out.push({
                    "time": parts[i * 2],
                    "message": parts[i * 2 + 1]
                })
            }
            decodeURI(json["systemMessages"]).split("|").forEach
            json["systemMessages"] = out
        } else {
            json["systemMessages"] = []
        }
        return json;
    } catch (error) { console.error("Error:", error); }
}

// Returns the different states and possible transitions
async function getStateMachine() {
    try {
        const xml = await get('getStateMachine', lid = LID_GATEWAY);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

async function getRunInfo(run = 0) {
    try {
        const xml = await get("getRunInfo&RunNumber=" + run.toString(),
            lid = LID_GATEWAY);
        let out = xmlToJson(xml).DATA;
        //console.log(out["plugin"])
        if (out["plugin"]) {
            out["plugin"] = JSON.parse(out["plugin"])
        }
        return out
    } catch (error) { console.error("Error:", error); }
}

// transition the state machine
async function transition(state, config = "crv_vst_config", name = "CrvVstRun") {
    try {
        const xml = await get(state + "&fsmName=" + name + "&fsmWindowName=Mu2e",
            data = "ConfigurationAlias=" + config,
            lid = LID_GATEWAY, type1 = "transition");
        let json = xmlToJson(xml).DATA;
        if (json['state_tranisition_attempted'] != "1") {
            throw (json['state_tranisition_attempted_err']);
        }
        return json;
    } catch (error) { console.error("Error:", error); }
}

async function transitionThis(state, name = "CrvVstRun") {
    try {
        const res = await getActiveTableGroups();
        const alias = res["Configuration-ActiveGroupAlias"]
        console.log("Configure with " + alias)
        return transition(state, alias, name);
    } catch (error) { console.error("Error:", error); }
}

// Returns the names of all avaiable state machines
// not yet sure where this is used
async function getStateMachineNames() {
    try {
        const xml = await get('getStateMachineNames', lid = LID_GATEWAY);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

async function getCurrentState() {
    try {
        const xml = await get('getCurrentState', lid = LID_GATEWAY);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}


// lcount: last count?
async function getConsoleMsgs(lcount = 0) {
    try {
        const xml = await get('GetConsoleMsgs',
            data = "lcount=" + lcount.toString(), lid = LID_CONSOLE);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

// Slow Controls
async function getSlowPages(lcount = 0) {
    try {
        const xml = await get('GetPages',
            data = "lcount=" + lcount.toString(), lid = LID_SLOWCONTROLS);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

// Get the settings of PVS in a list
async function getPVSettings(pvlist = []) {
    try {
        const xml = await get('getPVSettings',
            data = "pvList=" + pvlist.join(",") + ",", lid = LID_SLOWCONTROLS);
        //console.log(xml);
        return JSON.parse(xmlToJson(xml).DATA.JSON);
    } catch (error) { console.error("Error:", error); }
}

async function getPVList() {
    try {
        const xml = await get('getList',
            data = "", lid = LID_SLOWCONTROLS);
        //console.log(xml);
        return JSON.parse(xmlToJson(xml).DATA.JSON);
    } catch (error) { console.error("Error:", error); }
}

// generate a uid associated with a list of PVs,
// this uid is used to poll the data
async function getPollUid(pvlist = []) {
    try {
        const xml = await get("generateUID",
            data = "pvList=" + pvlist.join(",") + ",", lid = LID_SLOWCONTROLS);
        return JSON.parse(xmlToJson(xml).DATA.JSON);
    } catch (error) { console.error("Error:", error); }
}

// needs uid from the function above to poll the data
async function pollPV(uid) {
    try {
        const xml = await get("poll&uid=" + uid.toString(),
            data = "", lid = LID_SLOWCONTROLS);
        return JSON.parse(xmlToJson(xml).DATA.JSON);
    } catch (error) { console.error("Error:", error); }
}

// same as poll but takes a pvlist instead of uid
async function getPvData(pvlist = []) {
    try {
        const xml = await get("getPvData",
            data = "pvList=" + pvlist.join(",") + ",", lid = LID_SLOWCONTROLS);
        return JSON.parse(xmlToJson(xml).DATA.JSON);
    } catch (error) { console.error("Error:", error); }
}

// if no pvlist is given, get all alarms
async function getLastAlarmsData(pvlist = []) {
    try {
        data = "pvList=" + pvlist.join(",") + ","
        const xml = await get("getLastAlarmsData",
            data = "", lid = LID_SLOWCONTROLS);
        console.log(xml);
        return JSON.parse(xmlToJson(xml).DATA.JSON);
    } catch (error) { console.error("Error:", error); }
}

// these exposes the internal alarm checks of the slow controls dashboard to the user
async function getAlarmChecks() {
    try {
        const xml = await get("getAlarmsCheck",
            data = "", lid = LID_SLOWCONTROLS);
        if (xml) return JSON.parse(xmlToJson(xml).DATA.JSON);
        else return undefined;
    } catch (error) { console.error("Error:", error); }
}

async function getTreeView(startPath = "//XDAQContextTable/CRV08FEContext") {
    try {
        const xml = await get("getTreeView&depth=15",
            data = "startPath=" + startPath, lid = LID_CONFIG);
        return xmlToJson(xml).DATA['tree'];
    } catch (error) { console.error("Error:", error); }
}

async function getHardwareTree(context = null) {
    try {
        if (context) {
            const xml = await get("getTreeView&depth=15",
                data = "startPath=//XDAQContextTable/" + context, lid = LID_CONFIG);
            //var tree = xml.getElementsByTagName("ROOT")[0].getElementsByTagName("DATA")[0].getElementsByTagName("tree")[0]
            let hardware = {};
            let dtcs = xml.querySelectorAll("[value='LinkToFEInterfaceTable']>node");
            dtcs.forEach(dtc => {
                const dtcName = dtc.getAttribute("value")
                const dtcStatus = dtc.querySelector("[value='Status']>value").getAttribute("value")
                const dtcIndex = dtc.querySelector("[value='DeviceIndex']>value").getAttribute("value")
                const dtcId = dtc.querySelector("[value='EventBuilderDTCID']>value").getAttribute("value")
                hardware[dtcName] = {
                    "status": dtcStatus,
                    "index": dtcIndex,
                    "id": dtcId,
                    "rocs": {}
                };
                let rocs = dtc.querySelectorAll("[value='LinkToROCGroupTable']>node");
                rocs.forEach(roc => {
                    const rocName = roc.getAttribute("value")
                    const rocStatus = roc.querySelector("[value='Status']>value").getAttribute("value")
                    const rocId = roc.querySelector("[value='linkID']>value").getAttribute("value")
                    hardware[dtcName]["rocs"][rocName] = {
                        "status": rocStatus,
                        "linkId": rocId,
                        "febs": {}
                    }
                    let febs = dtc.querySelectorAll("[value='LinkToFEBInterfaceTable']>node");
                    febs.forEach(feb => {
                        const febName = feb.getAttribute("value")
                        const febStatus = feb.querySelector("[value='Status']>value").getAttribute("value")
                        const febPort = feb.querySelector("[value='Port']>value").getAttribute("value")
                        hardware[dtcName]["rocs"][rocName]["febs"][febName] = {
                            "status": febStatus,
                            "port": febPort
                        }
                    });
                });
            });
            return hardware;
        } else {
            hardware = {};
            let context = "CRV08FEContext";
            const hw = await getHardwareTree(context = context);
            hardware[context] = hw;
            return hardware;
            //const contexts = getContextNames()['ContextMember'];
            //contexts.forEach(context => {
            //
            //})
        }
    } catch (error) { console.error("Error:", error); }
}


async function setTreeNodeFieldValues(table, uid, fields, values) {
    try {
        const xml = await get("setTreeNodeFieldValues&tableGroup=&tableGroupKey=-1",
            data = "startPath=/" + table + "&recordList=" + uid + "&valueList=" + values + "&fieldList=" + fields + "&modifiedTables=",
            lid = LID_CONFIG);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

async function getTreeNodeFieldValues(table, fields, uids) {
    try {
        const xml = await get("getTreeNodeFieldValues&tableGroup=&tableGroupKey=-1",
            data = "startPath=/" + table + "&recordList=" + uids + "&fieldList=" + fields + "&modifiedTables=",
            lid = LID_CONFIG);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

async function getSpecificTable(table) {
    try {
        const xml = await get("getSpecificTable&tableName=" + table + "&dataOffset=0&chunkSize=100",
            data = "CookieCode=" + CookieCode,
            lid = LID_CONFIG);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}


///////////////// used in saveConfig /////////////////
// TODO, add cookie code!
async function saveSpecificTable(table, version, comment = "auto save") {
    try {
        const xml = await get("saveSpecificTable&dataOffset=0&chunkSize=0&tableName=" + table + "&version=" + version +
            "&tableComment=" + encodeURIComponent(comment) + "&sourceTableAsIs=1&lookForEquivalent=1",
            data = "CookieCode=" + CookieCode,
            lid = LID_CONFIG);
        console.log(xmlToJson(xml).DATA)
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

async function saveNewTableGroup(group, tableList = "", comment = "test save", useCache = 0) {
    try {
        const xml = await get("saveNewTableGroup&groupName=" + group + "&allowDuplicates=0&lookForEquivalent=1&ignoreWarnings=0&groupComment=" + encodeURIComponent(comment) + "&reuseCache=" + useCache.toString(),
            data = "CookieCode=" + CookieCode + "&" +
            "tableList=" + tableList,
            lid = LID_CONFIG);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

async function activateTableGroup(group, key) {
    try {
        const xml = await get("activateTableGroup&groupName=" + group + "&groupKey=" + key.toString() + "&ignoreWarnings=1",
            data = "CookieCode=" + CookieCode,
            lid = LID_CONFIG);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

async function setGroupAliasInActiveBackbone(group, key, alias, aliasComment = "") {
    try {
        const xml = await get("setGroupAliasInActiveBackbone&groupName=" + group + "&groupKey=" + key.toString() + "&groupAlias=" + alias + "&aliasComment=" + aliasComment,
            data = "CookieCode=" + CookieCode,
            lid = LID_CONFIG);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

async function getActiveTableGroups() {
    try {
        const xml = await get("getActiveTableGroups",
            data = "CookieCode=" + CookieCode,
            lid = LID_CONFIG);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

// don't use, resets the configuration
async function getTableStructureStatus() {
    try {
        const xml = await get("getTableStructureStatusAsJSON",
            data = "CookieCode=" + CookieCode,
            lid = LID_CONFIG);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

async function getActiveTables() {
    try {
        const xml = await get("getActiveTables",
            data = "CookieCode=" + CookieCode,
            lid = LID_CONFIG);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

async function getSpecificTableGroup(group, key) {
    try {
        const xml = await get("getSpecificTableGroup&groupName=" + group + "&groupKey=" + key.toString(),
            data = "CookieCode=" + CookieCode,
            lid = LID_CONFIG);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

async function clearTableTemporaryVersions(table) {
    try {
        const xml = await get("clearTableTemporaryVersions&tableName=" + table,
            data = "CookieCode=" + CookieCode,
            lid = LID_CONFIG);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}


///////////////// used in saveConfig /////////////////

// Wrapper function to store a table group, activate it, and adjust the active alias to it
// the config argument is an object with table:version pairs
async function saveConfig(msgfunc = undefined) {
    console.log("saveConfig")
    try {
        const res = await getActiveTables();
        const ActiveTable = res["ActiveTable"];
        let ActiveTableVersion = res["ActiveTableVersion"];
        const group = res["Configuration-ActiveGroupName"]
        const key = Number(res["Configuration-ActiveGroupKey"])
        const alias = res["Configuration-ActiveGroupAlias"]
        const backbone = res["Backbone-ActiveGroupName"]
        let promises2 = [getSpecificTableGroup(group, key)]; // this one can run in parallel
        let promises = []
        for (let i = 0; i < Number(res["Number"]); i++) {
            //console.log(res["ActiveTable"][i], res["ActiveTableVersion"][i])
            if (Number(ActiveTableVersion[i]) < 0) {
                promises.push(saveSpecificTable(ActiveTable[i], ActiveTableVersion[i], comment = "save active config"));
                //if(msgfunc) msgfunc("save "+ActiveTable[i]);
            }
        }
        const results = await Promise.all(promises);


        // modify active table versions (somehow using getActiveTables doesn't work and clean up all temporary versions)
        results.forEach(res => {
            if ("savedName" in res) {
                const idx = ActiveTable.indexOf(res["savedName"])
                ActiveTableVersion[idx] = res["savedVersion"];
                clearTableTemporaryVersions(res["savedName"]);
                if (msgfunc) {
                    if ("foundEquivalentVersion" in res) {
                        msgfunc("Found equivalent version " + res["savedVersion"] + " for " + res["savedName"]);
                    } else {
                        msgfunc("Saved new " + res["savedName"] + "(" + res["savedVersion"] + ") ");
                    }
                }
            }
        });
        //let promises2 = [getActiveTables(), getSpecificTableGroup(group, key)];
        const res2 = await Promise.all(promises2);
        let tableList = ""
        res2[0]["TableGroupMembers"]["MemberName"].forEach(el => {
            tableList += el + ",";
            const index = ActiveTable.findIndex(item => item === el);
            tableList += ActiveTableVersion[index] + ","
        });
        //if(msgfunc) msgfunc("save new "+group);

        const newGroup = await saveNewTableGroup(group, tableList, "save active config", 1);
        const newKey = Array.isArray(newGroup["TableGroupKey"]) ? newGroup["TableGroupKey"][0] : newGroup["TableGroupKey"];
        if (newKey == undefined) {// Something went wrong, abort
            if (msgfunc) {
                msgfunc("ERROR: generating a new group table failed.")
                msgfunc(newGroup["TreeErrors"])
            }
            return;
            //throw "Failed to save the active configuration."
        }
        if (msgfunc) {
            if (Array.isArray(newGroup["TableGroupKey"])) msgfunc("found equivalent version " + newGroup["TableGroupKey"][0] + " for " + group);
            else msgfunc("new " + group + " version " + newGroup["TableGroupKey"] + " created");
        }
        const foo = await activateTableGroup(group, newKey);
        if (msgfunc) msgfunc(group + " (" + newKey.toString() + ") activated");

        const newBackboneKey = await updateBackbone(backbone, group, newKey, alias, "save active config", msgfunc);

        return { group: newKey, backbone: newBackboneKey }
    } catch (error) {
        if (msgfunc) msgfunc(error);
        console.error(error)
    }
}

async function updateBackbone(backbone, group, key, alias, comment = "", msgfunc = undefined) {
    const newAlias = await setGroupAliasInActiveBackbone(group, key, alias, comment);
    const VersionAliasesVersion = newAlias["oldBackboneVersion"][1]
    const GroupAliasesTable = newAlias["savedVersion"]
    const backboneTableList = "GroupAliasesTable," + GroupAliasesTable + ",VersionAliasesTable," + VersionAliasesVersion + ","
    const newBackbone = await saveNewTableGroup(backbone, backboneTableList, "save latest backbone", 1);
    const newBackboneKey = Array.isArray(newBackbone["TableGroupKey"]) ? newBackbone["TableGroupKey"][0] : newBackbone["TableGroupKey"];
    const bar = await activateTableGroup(backbone, newBackboneKey);
    if (msgfunc) {
        if ("foundEquivalentKey" in newBackbone) {
            msgfunc("found equivalent backbone version " + newBackboneKey);
        } else {
            msgfunc("new backbone version " + newBackboneKey + " activated");
        }
    }
    return newBackboneKey;
}

async function reloadConfig(msgfunc = undefined) {
    const res = await getTableStructureStatus(); // this call resets everything to the last config
    if (msgfunc) {
        const res2 = await getActiveTables();
        const alias = res2["Configuration-ActiveGroupAlias"]
        const group = res2["Configuration-ActiveGroupName"]
        const key = Number(res2["Configuration-ActiveGroupKey"])
        msgfunc("Reloaded " + alias + ": " + group + "(" + key.toString() + ")")
    }
}

async function setAlias(alias, comment, msgfunc = undefined) {
    try {
        const res = await getActiveTableGroups();
        const group = res["Configuration-ActiveGroupName"]
        const key = Number(res["Configuration-ActiveGroupKey"])
        const backbone = res["Backbone-ActiveGroupName"]
        const newBackboneKey = await updateBackbone(backbone, group, key, alias, comment, msgfunc);
        return { group: key, backbone: newBackboneKey }
    } catch (error) {
        if (msgfunc) msgfunc(error);
        console.error(error)
    }
}

async function getDAQReport() {
    try {
        const xml = await get("getDAQReport",
            data = "", lid = LID_TFM);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

async function getDAQState() {
    try {
        const xml = await get("getDAQState",
            data = "", lid = LID_TFM);
        json = xmlToJson(xml).DATA;
        for (const key in json) {
            if (json[key][0] == "{")
                json[key] = JSON.parse(json[key].replace('\"', '"'));
        }
        return json;
    } catch (error) { console.error("Error:", error); }
}

async function dtcRead(reg, dtc = "daq08DTC") {
    try {
        const xml = await get("runFEMacro&feClassSelected=DTCFrontEndInterface&feUIDSelected=" + dtc + "&macroType=fe&macroName=DTC%20Read&saveOutputs=0",
            data = "inputArgs=address," + reg + "&outputArgs=readData", lid = LID_MACROMAKER);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

async function dtcWrite(reg, val, dtc = "daq08DTC") {
    try {
        const xml = await get("runFEMacro&feClassSelected=DTCFrontEndInterface&feUIDSelected=" + dtc + "&macroType=fe&macroName=DTC%20Write&saveOutputs=0",
            data = "inputArgs=address," + reg + ";writeData," + val + "&outputArgs=Status",
            lid = LID_MACROMAKER);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

async function rocRead(reg, dtc = "daq08DTC", link = 0) {
    try {
        const xml = await get("runFEMacro&feClassSelected=DTCFrontEndInterface&feUIDSelected=" + dtc + "&macroType=fe&macroName=ROC%20Read&saveOutputs=0",
            data = "inputArgs=rocLinkIndex," + link.toString() + ";address," + reg + "&outputArgs=readData", lid = LID_MACROMAKER);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

async function rocWrite(reg, val, dtc = "daq08DTC", link = 0) {
    try {
        const xml = await get("runFEMacro&feClassSelected=DTCFrontEndInterface&feUIDSelected=" + dtc + "&macroType=fe&macroName=ROC%20Write&saveOutputs=0",
            data = "inputArgs=rocLinkIndex," + parseInt(link).toString() + ";address," + reg + ";writeData," + val + "&outputArgs=",
            lid = LID_MACROMAKER);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

async function dtcReset(hard = false, dtc = "daq08DTC") {
    try {
        const macroName = hard ? "DTC%20Hard%20Reset" : "DTC%20Soft%20Reset"
        const xml = await get("runFEMacro&feClassSelected=DTCFrontEndInterface&feUIDSelected=" + dtc + "&macroType=fe&macroName=" + macroName + "&saveOutputs=0",
            data = "inputArgs=&outputArgs=",
            lid = LID_MACROMAKER);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

async function rocReset(dtc = "daq08DTC", roc = "Default") {
    try {
        const xml = await get("runFEMacro&feClassSelected=DTCFrontEndInterface&feUIDSelected=" + dtc + "&macroType=fe&macroName=ROC%20FEMacro%20-%20Reset%20uC&saveOutputs=0",
            data = "inputArgs=Target%20ROC%20(Default%20%3D%20-1%20%3A%3D%20all%20ROCs)," + roc + "&outputArgs=Target%20ROC",
            lid = LID_MACROMAKER);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

async function rocPwrPort(dtc = "daq08DTC", roc = "Default", port = "Default") {
    try {
        const xml = await get("runFEMacro&feClassSelected=DTCFrontEndInterface&feUIDSelected=" + dtc + "&macroType=fe&macroName=ROC%20FEMacro%20-%20PWRRST&saveOutputs=0",
            data = "inputArgs=Target%20ROC%20(Default%20%3D%20-1%20%3A%3D%20all%20ROCs)," + roc + ";port%20(Default%2025%20-%20all)," + port + "&outputArgs=Target%20ROC",
            lid = LID_MACROMAKER);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

async function febCMBENA(val = "1", dtc = "daq08DTC", roc = "Default") {
    try {
        const xml = await get("runFEMacro&feClassSelected=DTCFrontEndInterface&feUIDSelected=" + dtc + "&macroType=fe&macroName=ROC%20FEMacro%20-%20FEBs%20CMBENA&saveOutputs=0",
            data = "inputArgs=Target%20ROC%20(Default%20%3D%20-1%20%3A%3D%20all%20ROCs)," + roc + ";value%20(Default%201)," + val + "&outputArgs=Target%20ROC",
            lid = LID_MACROMAKER);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

async function febSetBias(val, fpga, no, dtc = "daq08DTC", roc = "Default", port = "Default") {
    try {
        const xml = await get("runFEMacro&feClassSelected=DTCFrontEndInterface&feUIDSelected=" + dtc + "&macroType=fe&macroName=ROC%20FEMacro%20-%20FEB%20Set%20Bias&saveOutputs=0",
            data = "inputArgs=Target%20ROC%20(Default%20%3D%20-1%20%3A%3D%20all%20ROCs)," + roc + ";port%20(Default%3A%20-1%2C%20current%20active)," + port + ";fpga%20%5B0%2C1%2C2%2C3%5D," + fpga + ";number%20%5B0%2C1%5D," + no + ";bias," + val + "&outputArgs=Target%20ROC",
            lid = LID_MACROMAKER);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

async function sendEventsDTC(dtc = "daq08DTC", interval_us = 5., n = 12000) {
    data = "inputArgs=Enable%20CFO%20Emulator%20(Default%20%3A%3D%20false),true;";
    data += "Fixed-width%20Event%20Window%20Duration%20(s%2C%20ms%2C%20us%2C%20ns%2C%20and%20clocks%20allowed)%20%5Bclocks%20%3A%3D%2025ns%5D," + interval_us.toString() + "%20us;";
    data += "Number%20of%20Event%20Window%20Markers%20to%20generate%20(0%20%3A%3D%20infinite)," + n.toString() + ";";
    data += "Starting%20Event%20Window%20Tag,0x1;";
    data += "Event%20Window%20Mode%20(Default%20%3A%3D%201),0x19;";
    data += "Enable%20Auto-generation%20of%20Data%20Request%20Packets%20(Default%20%3A%3D%20false),true;";
    data += "Enable%20Clock%20Markers%20(Default%20%3A%3D%20false),false;";
    data += "Use%20Detached%20Buffer%20Test%20(Default%20%3A%3D%20false),false;";
    data += "For%20Detached%20Buffer%20Test%2C%20Save%20Binary%20Data%20to%20File%20(Default%3A%20false),false;";
    data += "For%20Detached%20Buffer%20Test%2C%20Save%20Subevent%20Header%20to%20Binary%20File%20(Default%3A%20false),false;";
    data += "For%20Detached%20Buffer%20Test%2C%20Do%20NOT%20Reset%20Counters%20(Default%3A%20false),false";
    data += "&outputArgs=response";
    console.log(data)
    try {
        const xml = await get("runFEMacro&feClassSelected=DTCFrontEndInterface&feUIDSelected=" + dtc + "&macroType=fe&macroName=CFO%20Emulator%20Fixed-width%20Event%20Window%20Emulation%20Setup&saveOutputs=0",
            data = data,
            lid = LID_MACROMAKER);
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

async function getHistograms(port, fpga, channel, interval, nbins, dtc = "daq08DTC") {
    try {
        const xml = await get("runFEMacro&feClassSelected=DTCFrontEndInterface&feUIDSelected=" + dtc + "&macroType=fe&macroName=ROC%20FEMacro%20-%20Histogram&saveOutputs=0",
            data = "inputArgs=Target%20ROC%20(Default%20%3D%20-1%20%3A%3D%20all%20ROCs),0;port%20(Default%3A%20-1%2C%20current%20active)," + port +
            ";fpga%20%5B0%2C1%2C2%2C3%5D," + fpga +
            ";channel%20%5B0-15%5D," + channel +
            ";interval%20(Default%202s)%20%5Bms%5D," + interval +
            ";filename%20(Default%3A%20histogram.csv),Default" +
            ";number%20of%20bins%20(Default%20all:%200x400)," + nbins +
            "&outputArgs=Target%20ROC,buffer",
            lid = LID_MACROMAKER);
        //console.log(xmlToJson(xml).DATA)
        return xmlToJson(xml).DATA;
    } catch (error) { console.error("Error:", error); }
}

function mu2e_init(name) {
    document.title = "Mu2e :: " + name
    let active = name
    switch (name) {
        case "DTC":
            loadDTC();
            active = "DTC-" + _dtcName
            break;
        case "ROC":
            loadROC();
            active = "ROC-" + _rocName
            break;
        case "FEB":
            loadFEB();
            active = "FEB-" + _rocName
            break;
        case "RunLog":
            let n = new URLSearchParams(window.location.search).get('n')
            if (n == undefined) n = 20;
            loadRunLog(n);
            active = "RunLog"
            break;
        default:
    }
    updateHeader();
    updateAliasList(); // don't update regularly
    addNav(); // adds main navigation entries like Overview
    updateAppStatus();
    updateHardware();
    updateArtdaq();
    updateAlarms();
    updateMessages();
    updateRunInfo();
    updateConfig();
    updateNav(active); // sets the active navigation entry

    loadDcsChannels(); // scans the document for <div name="mu2e_dcs" data="CHANNEL-NAME">
    loadConfigChannels(); // scans the document for <span class="mu2e_config" path="/TABLE" uid="UID" field="FIELD">
    fetchData();
    setInterval(fetchData, 1000);
    // start

}

mu2e_dcs_channels = []
function loadDcsChannels() {
    let dcs = document.querySelectorAll("div[name=\"mu2e_dcs\"]")
    for (let i = 0; i < dcs.length; i++) {
        let channel = dcs[i].getAttribute("data");
        if (channel) {
            mu2e_dcs_channels.push(channel)
        }
    }
    if (mu2e_dcs_channels.length > 0) handleEPICS();
}

mu2e_config_fields = {}
function loadConfigChannels() {
    let configs = document.querySelectorAll("span[class=\"mu2e_config\"]")
    for (let i = 0; i < configs.length; i++) {
        let path = configs[i].getAttribute("path");
        let uid = configs[i].getAttribute("uid");
        let field = configs[i].getAttribute("field");
        if (path) {
            if (!(path in mu2e_config_fields)) mu2e_config_fields[path] = { "uids": [], "fields": [] }
            mu2e_config_fields[path]["uids"].push(uid)
            mu2e_config_fields[path]["fields"].push(field)
        }
    }
    if (configs.length > 0) handleConfig();
}

var _dtcName = undefined;
function loadDTC() {
    const dtcName = new URLSearchParams(window.location.search).get('dtc')
    if (dtcName) {
        document.querySelector("div[id=\"mu2e_dtc\"]>div").textContent = "DTC - " + dtcName
    }
    document.querySelectorAll("div[name='mu2e_dcs']").forEach(div => {
        div.setAttribute("data", DCS_PREFIX + ":" + dtcName + ":" + div.getAttribute("data"))
    });
    _dtcName = dtcName;
}

var _rocName = undefined;
function loadROC() {
    const rocName = new URLSearchParams(window.location.search).get('roc')
    const dtcName = new URLSearchParams(window.location.search).get('dtc')
    const dtcLink = new URLSearchParams(window.location.search).get('link')
    if (rocName) {
        const div = document.querySelector("div[id=\"mu2e_roc\"]>div")
        if (div) div.textContent = "ROC - " + rocName
    }
    if ((dtcName) && (dtcLink)) {
        const div_dtc = document.querySelector("#dtc")
        if (div_dtc) div_dtc.textContent = dtcName + ":" + dtcLink
    }

    document.querySelectorAll("div[name='mu2e_dcs']").forEach(div => {
        div.setAttribute("data", DCS_PREFIX + ":" + rocName + ":" + div.getAttribute("data"))
    });
    _rocName = rocName;
}

async function loadFEB() {
    //const febName = new URLSearchParams(window.location.search).get('feb')
    const rocName = new URLSearchParams(window.location.search).get('roc')
    const rocPort = new URLSearchParams(window.location.search).get('port')
    const febUid = new URLSearchParams(window.location.search).get('feb')
    //const dtcName = new URLSearchParams(window.location.search).get('dtc')
    if (rocName) {
        const div = document.querySelector("div[id=\"mu2e_feb\"]>div")
        if (div) div.textContent = febUid + " - " + rocName + " - port " + rocPort
    }
    document.querySelectorAll("div[name='mu2e_dcs']").forEach(div => {
        div.setAttribute("data", DCS_PREFIX + ":" + rocName + ":FEB_p" + rocPort + "_" + div.getAttribute("data"))
    });

    // Configuration
    let div_groups = document.querySelector("[id=mu2e_feb_groups]");
    let div_chs = document.querySelector("[id=mu2e_feb_channels");
    if ((div_groups != undefined) || (div_chs != undefined)) {
        const res = await getTreeView("/FEBInterfaceTable/" + febUid);
        const node = res["/FEBInterfaceTable/FEB0"]["node"];

        const groups = node[node.findIndex(obj => "LinkToCRVChannelGroupTable" in obj)]["LinkToCRVChannelGroupTable"]["node"]
        const channels = node[node.findIndex(obj => "LinkToCRVChannelTable" in obj)]["LinkToCRVChannelTable"]["node"]

        // add the group settings
        if (div_groups) {
            groups.forEach(g => {
                const uid = Object.keys(g)[0];
                const t = g[uid]["node"];
                const number = Number(Object.keys(t[1])[0].split(",")[1]);
                const bias = Object.keys(t[2])[0].split(",")[1];

                let bias_div = getTableDiv(6, (4 + number * 2));

                let span = document.createElement("span"); // add to be able to edit
                span.setAttribute("path", "/SubsystemCRVChannelGroupTable");
                span.setAttribute("uid", uid);
                span.setAttribute("field", "bias");

                let a = document.createElement("a")
                a.classList.add("mu2e_editable")
                a.href = "#"
                a.addEventListener('click', function (event) { enableEdit(event.target); })
                a.innerHTML = bias;
                let rfloat = document.createElement("span")
                rfloat.classList.add("mu2e_right_float")
                rfloat.innerHTML = uid
                span.appendChild(a);
                bias_div.appendChild(span)
                bias_div.appendChild(rfloat)
                div_groups.appendChild(bias_div);
            });
        }

        if (div_chs) {
            let ridx = 3;
            channels.forEach(ch => {
                const chUid = Object.keys(ch)[0];
                const channel = Object.keys(ch[chUid]["node"][1])[0].split(",")[1]
                const biasTrim = Object.keys(ch[chUid]["node"][2])[0].split(",")[1]
                const threshold = Object.keys(ch[chUid]["node"][3])[0].split(",")[1]
                let ch_div = getTableDiv(2, ridx); ch_div.classList.add("mu2e_collapsable");
                let biasTrim_div = getTableDiv(3, ridx); biasTrim_div.classList.add("mu2e_collapsable");
                let th_div = getTableDiv(4, ridx); th_div.classList.add("mu2e_collapsable");
                ch_div.style.display = "none";
                biasTrim_div.style.display = "none";
                th_div.style.display = "none";
                ch_div.innerHTML = Number(channel).toString().padStart(2, '0') + " (" + chUid + ")"

                let span = document.createElement("span"); // add to be able to edit
                span.setAttribute("path", "/SubsystemCRVChannelTable");
                span.setAttribute("uid", chUid);
                span.setAttribute("field", "BiasTrim");
                let a = document.createElement("a");
                a.href = "#"
                a.addEventListener('click', function (event) { enableEdit(event.target); })
                a.innerHTML = biasTrim
                span.appendChild(a)
                biasTrim_div.appendChild(span)

                let span2 = document.createElement("span"); // add to be able to edit
                span2.setAttribute("path", "/SubsystemCRVChannelTable");
                span2.setAttribute("uid", chUid);
                span2.setAttribute("field", "Threshold");
                let a2 = document.createElement("a");
                a2.href = "#"
                a2.addEventListener('click', function (event) { enableEdit(event.target); })
                a2.innerHTML = threshold
                span2.appendChild(a2)
                th_div.appendChild(span2)

                div_chs.appendChild(ch_div)
                div_chs.appendChild(biasTrim_div)
                div_chs.appendChild(th_div)
                ridx++;
            });
        }
    }
}

function getTableDiv(col, row) {
    let div = document.createElement("div")
    div.classList.add("mu2e_list")
    div.style.cssText = "grid-column: " + col.toString() + "; grid-row: " + row.toString() + ";"
    return div;
}

function setStatusColor(el, className) {
    el.classList.remove("mu2e_bad")
    el.classList.remove("mu2e_bad_text")
    el.classList.remove("mu2e_ok")
    el.classList.remove("mu2e_warning")
    el.classList.remove("mu2e_transition")
    el.classList.remove("mu2e_busy")
    if (className)
        el.classList.add(className)
}

async function fetchData() {
    //clearMessages()

    // send all enabled fetch requests
    if (getAppStatusEnabled) handleAppStatus();
    if (getCurrentStateEnabled) handleCurrentState();
    if (mu2e_dcs_channels.length > 0) handleEPICS();
    //if(mu2e_config_fields.length > 0) handleConfig();
    if (getAlarmChecksEnabled) handleAlarms();
    if (getSystemMessagesEnabled) handleMessages();
    //if(getAliasListEnabled) handleAliasList();
    if (getArtdaqEnabled) handleArtdaq();
}

async function updateHardware() {
    let res = await getHardwareTree();

    // update navigation always when present
    let nav = document.querySelector("div[id='mu2e_nav']")
    if (nav) {
        let div_dtc = document.createElement("div")
        let div_roc = document.createElement("div")
        let span_title = document.createElement("span")
        span_title.innerHTML = "Hardware"
        div_dtc.appendChild(span_title)
        Object.keys(res).forEach(contextName => {
            Object.keys(res[contextName]).forEach(dtcName => {
                Object.keys(res[contextName][dtcName]["rocs"]).forEach(rocName => {
                    //console.log(res[contextName][dtcName]["rocs"][rocName])
                    const link = res[contextName][dtcName]["rocs"][rocName]["linkId"]
                    let a_roc = document.createElement("a")
                    a_roc.href = "Mu2eROC.html?roc=" + rocName + "&dtc=" + dtcName + "&link=" + link.toString()
                    a_roc.innerHTML = "- " + rocName
                    a_roc.id = "ROC-" + rocName
                    if (rocName == _rocName) {
                        a_roc.classList.add("mu2e_nav_active")
                    }
                    div_roc.appendChild(a_roc)
                });
                let a_dtc = document.createElement("a")
                a_dtc.href = "Mu2eDTC.html?dtc=" + dtcName
                a_dtc.innerHTML = "- " + dtcName
                a_dtc.id = "DTC-" + dtcName
                if (dtcName == _dtcName) {
                    a_dtc.classList.add("mu2e_nav_active")
                }
                div_dtc.appendChild(a_dtc)
            });
        });
        nav.appendChild(div_dtc)
        nav.appendChild(div_roc)
    }

    // if mu2e_hardware div exists, also update that
    let div = document.getElementById("mu2e_hardware");
    if (div == undefined) return;
    div.replaceChildren()

    const title = document.createElement("div")
    title.classList.add("mu2e_title")
    title.style.cssText = "grid-column: 1/5; grid-row: 1";
    title.textContent = "Hardware"
    //span.appendChild(toggle)
    //title.appendChild(span)
    div.appendChild(title)

    let row_idx = 2;
    Object.keys(res).forEach(contextName => {
        const context = res[contextName];
        c_rowidx_start = row_idx;
        Object.keys(context).forEach(dtcName => {
            const dtc = context[dtcName];
            const dtc_status = dtc["status"]
            const dtc_id = dtc['id']
            d_rowidx_start = row_idx;
            Object.keys(dtc["rocs"]).forEach(rocName => {
                r_rowidx_start = row_idx;
                const roc = dtc["rocs"][rocName];
                let link = roc["linkId"]
                Object.keys(roc["febs"]).forEach(febName => {
                    let feb = roc["febs"][febName]
                    let f = document.createElement("div")
                    f.style.cssText = "grid-column: 4; grid-row:" + (row_idx++).toString() + ";";
                    f.classList.add("mu2e_list")
                    if (feb['status'] != "On")
                        f.classList.add("mu2e_disabled")
                    let feb_a = document.createElement("a")
                    feb_a.href = "Mu2eFEB.html?roc=" + rocName + "&dtc=" + dtcName + "&link=" + link.toString() + "&port=" + feb['port'].toString() + "&feb=" + febName
                    feb_a.innerHTML = febName
                    f.appendChild(feb_a)
                    f.innerHTML = "port-" + feb['port'].toString() + ": " + f.innerHTML
                    div.appendChild(f);
                    let f2 = document.createElement("div")
                    f2.style.cssText = "grid-column: 4; grid-row:" + (row_idx++).toString() + ";";
                    f2.classList.add("mu2e_list")
                    //if(feb['status'] != "On")
                    f2.classList.add("mu2e_disabled")
                    f2.innerHTML = "port-" + feb['port'].toString() + ": " + febName + " TEST"
                    div.appendChild(f2);
                });
                let r = document.createElement("div")
                r.style.cssText = "grid-column: 3; grid-row:" + (r_rowidx_start).toString() + "/" + (row_idx).toString() + ";";
                r.classList.add("mu2e_list")
                let a = document.createElement("a")
                a.href = "Mu2eROC.html?roc=" + rocName + "&dtc=" + dtcName + "&link=" + link.toString()
                a.innerHTML = rocName
                r.appendChild(a)
                r.innerHTML = "link-" + roc["linkId"] + ": " + r.innerHTML
                div.appendChild(r);
            });
            let d = document.createElement("div")
            d.style.cssText = "grid-column: 2; grid-row:" + (d_rowidx_start).toString() + "/" + (row_idx).toString() + ";";
            d.classList.add("mu2e_list")
            let a = document.createElement("a")
            a.href = "Mu2eDTC.html?dtc=" + dtcName
            a.innerHTML = dtcName
            d.appendChild(a)
            d.innerHTML += " (id: " + dtc["id"] + ")"
            div.appendChild(d);
        });
        let c = document.createElement("div")
        c.style.cssText = "grid-column: 1; grid-row:" + (c_rowidx_start).toString() + "/" + (row_idx).toString() + ";";
        c.classList.add("mu2e_list")
        c.innerHTML = contextName
        div.appendChild(c);
    });
}

async function updateAppStatus(includeApps = false) {
    let div = document.getElementById("mu2e_apps");
    if (div == undefined) return;
    div.replaceChildren()

    const title = document.createElement("div")
    title.classList.add("mu2e_title")
    title.style.cssText = "grid-column: 1/" + (includeApps ? "6" : "5") + "; grid-row: 1";

    title.textContent = "XDAQ"
    let span = document.createElement("span")
    span.classList.add("mu2e_right_float");
    let refresh = document.createElement("a")
    refresh.href = "#";
    refresh.alt = "Refresh";
    refresh.style.cssText = "text-decoration:none; padding:3px; color:white;"
    refresh.addEventListener("click", function (e) { updateAppStatus(includeApps); });
    refresh.innerHTML = "&#x21bb";
    let toggle = document.createElement("a")
    toggle.href = "#";
    toggle.style.cssText = "text-decoration:none; padding:3px; color:white;"
    toggle.addEventListener("click", function (e) { updateAppStatus(!includeApps); });
    if (includeApps) {
        toggle.alt = "remove Apps";
        toggle.innerHTML = "-";
    } else {
        toggle.alt = "add Apps";
        toggle.innerHTML = "+";
    }

    //span.appendChild(refresh)
    span.appendChild(toggle)
    title.appendChild(span)
    div.appendChild(title)

    let res = await getAppStatus();
    // get all contexts
    let contexts = {}
    for (const appName in res) {
        const context_ = res[appName]['context'];
        if (!(context_ in contexts)) {
            contexts[context_] = { "url": res[appName]["url"], "apps": [] }
        }
    }

    let index = 3;
    Object.entries(contexts).forEach(([context, cdata]) => {
        //let row = document.createElement("div");
        let row_start = index;

        Object.keys(res).forEach(appN => {
            const app = res[appN];
            if (app['context'] == context) {
                contexts[context]['apps'].push(appN)
                let colindex = 3;
                if (includeApps) {
                    appName = document.createElement("div");
                    appName.style.cssText = "grid-column: " + (colindex++).toString() + "; grid-row:" + (index).toString() + ";";
                    appName.classList.add("mu2e_list")
                    appName.innerHTML = appN
                    div.appendChild(appName);
                }
                appStatus = document.createElement("div");
                appStatus.style.cssText = "grid-column: " + (colindex++).toString() + "; grid-row:" + (index).toString() + ";";
                appStatus.setAttribute("name", "mu2e_" + appN + "_state")
                appStatus.classList.add("mu2e_data")
                appStatus.innerHTML = app['status']
                if (includeApps || contexts[context]['apps'].length == 1) {
                    div.appendChild(appStatus);
                }
                appTime = document.createElement("div");
                appTime.style.cssText = "grid-column: " + (colindex++).toString() + "; grid-row:" + (index).toString() + ";";
                appTime.setAttribute("name", "mu2e_" + appN + "_state_time")
                appTime.classList.add("mu2e_data")
                appTime.innerHTML = formatTime(Number(app['stale']), noHours = true);
                if (includeApps || contexts[context]['apps'].length == 1) {
                    div.appendChild(appTime);
                }
                //if(contexts[context]['apps'].length > 1) {
                //    appTime.classList.add("group_"+context)
                //    appTime.classList.add("mu2e_hidden")
                //}
                //appDetails = document.createElement("div");
                //appDetails.style.cssText = "grid-column: 5; grid-row:"+(index).toString()+";";
                //appDetails.setAttribute("name", "mu2e_"+appN+"_details")
                //appDetails.classList.add("mu2e_data")
                //appDetails.innerHTML = app['detail'];
                //div.appendChild(appDetails);
                if (includeApps || contexts[context]['apps'].length == 1) {
                    index = index + 1;
                }
            }

        });
        let contextName = document.createElement("div");
        contextName.style.cssText = "grid-column: 1; grid-row:" + (row_start).toString() + "/" + (index).toString() + ";";
        contextName.classList.add("mu2e_list")
        contextName.innerHTML = context; //&#x21bb;
        div.appendChild(contextName);
        let contextHost = document.createElement("div");
        contextHost.style.cssText = "grid-column: 2; grid-row:" + (row_start).toString() + "/" + (index).toString() + ";";
        contextHost.classList.add("mu2e_list")
        const hostUrl = new URL(contexts[context]['url']);
        const hostName = hostUrl.hostname.split(".")[0] + ":" + hostUrl.port
        contextHost.innerHTML = hostName + "<span class=\"mu2e_right_float\"><a href=\"#\" style=\"text-decoration:none;\" alt=\"Restart\" onClick='restartContext(\"" + context + "\")'>&#x23FB;</a></span>"; //&#x21bb;, &#x2622;
        div.appendChild(contextHost);

    });

}

async function handleAppStatus() {
    let res = await getAppStatus();
    //console.log(res)

    // update all mu2e_NAME_status and mu2e_NAME_status_time elements
    Object.keys(res).forEach(appName => {
        const selector = 'div[name="mu2e_' + appName + '_state"]'
        const selectorTime = 'div[name="mu2e_' + appName + '_state_time"]'
        const status = res[appName]['status'].split(":::")
        document.querySelectorAll(selector).forEach(div => {
            let status_ = status[0];
            let progress = res[appName]['progress'];
            if (progress != "100") {
                status_ += " - " + progress + "%"
            }
            div.innerHTML = status_;
            if ((status[0] == "Running") || (status[0] == "Configured") || (status[0] == "Configuring")) {
                setStatusColor(div, "mu2e_ok");
            } else if ((status[0] == "Halted") || (status[0] == "Initial")) {
                setStatusColor(div, "mu2e_warning");
            } else {
                setStatusColor(div, "mu2e_bad");
            }
            if (status.length > 1) { // additional error message avaiable
                const newSpan = document.createElement('span');
                newSpan.innerHTML = status.join(":::");
                newSpan.className = "tooltiptext"
                div.appendChild(newSpan);
            }
        });
        time = res[appName]['stale']
        document.querySelectorAll(selectorTime).forEach(div => {
            div.innerHTML = formatTime(Number(time), noHours = true);
            if (Number(time) > 10) {
                setStatusColor(div, "mu2e_bad");
            } else {
                setStatusColor(div, null);
            }
            if (status.length > 1) { // additional error message avaiable
                const newSpan = document.createElement('span');
                newSpan.innerHTML = status.join(":::");
                newSpan.className = "tooltiptext"
                div.appendChild(newSpan);
            }
        });
    });

    //// set the status in the header
    if ('GatewaySupervisor' in res) {
        let status = res['GatewaySupervisor']['status'].split(":::")
        //    document.querySelectorAll('div[name="mu2e_state"]').forEach(div => {
        //        let status_ = status[0];
        //        let progress = res['GatewaySupervisor']['progress'];
        //        if(progress != "100") {
        //            status_ += " - "+progress+"%"
        //        }
        //        div.innerHTML = status_;
        //        if((status[0] == "Running") || (status[0] == "Configured")) {
        //            setStatusColor(div, "mu2e_ok");
        //        } else if((status[0] == "Halted") || (status[0] == "Init")) {
        //            setStatusColor(div, "mu2e_warning");
        //        } else {
        //            setStatusColor(div, "mu2e_bad");
        //        }
        //        if(status.length>1) { // additional error message avaiable
        //            const newSpan = document.createElement('span');
        //            newSpan.innerHTML = status.join(":::");
        //            newSpan.className = "tooltiptext"
        //            div.appendChild(newSpan);
        //        }
        //    });
        // adjust run transition buttons accordingly
        document.querySelectorAll('div[name="mu2e_transition"]').forEach(div => {
            let oldButton = div.querySelector('button')
            let button = document.createElement("button")
            let updated = false;
            switch (status[0]) {
                case "Configured":
                    if (oldButton == null || oldButton.innerHTML !== "Start") {
                        button.innerHTML = "Start"
                        button.addEventListener("click", function (e) {
                            try { transition("Start"); }
                            catch (error) {
                                const newSpan = document.createElement('span');
                                newSpan.innerHTML = error;
                                newSpan.className = "tooltiptext"
                                div.appendChild(newSpan);
                            };
                        });
                        button.disabled = false;
                        updated = true;
                    }
                    // add link to reconfigure
                    let state = document.querySelector("#state");
                    if (state) {
                        state.innerHTML +=
                            "<span class=\"mu2e_right_float\"><a href=\"#\" style=\"text-decoration:none;\" alt=\"Reconfigure\" onClick='transition(\"Halt\")'>&#x21bb</a></span>";
                    }
                    break;
                case "Halted":
                    if (oldButton == null || oldButton.innerHTML !== "Configure") {
                        button = button.cloneNode(false);
                        button.innerHTML = "Configure"
                        button.addEventListener("click", function (e) { transitionThis('Configure').then(res => { updateAliasList() }) });
                        button.disabled = false;
                        updated = true;
                    }
                    break;
                case "Running":
                    if (oldButton == null || oldButton.innerHTML !== "Stop") {
                        button = button.cloneNode(false);
                        button.innerHTML = "Stop"
                        button.addEventListener("click", function (e) { transition('Stop'); });
                        button.disabled = false;
                        updated = true;
                    }
                    break;
                case "Failed":
                case "Initial":
                    if (oldButton == null || oldButton.innerHTML !== "Halt") {
                        button = button.cloneNode(false);
                        button.innerHTML = "Halt"
                        button.addEventListener("click", function (e) { transition('Halt'); });
                        //button.disabled = false;
                        updated = true;
                    }
                    button.disabled = false;
                    break;
                default:
                    if (oldButton != null) {
                        oldButton.disabled = true;
                    }
            }
            if (updated) {
                if (oldButton) div.replaceChild(button, oldButton);
                else div.appendChild(button)
                updateRunInfo();
            }

        });

        if (res['GatewaySupervisor']['progress'] != '100') {
            let stale_ = res['GatewaySupervisor']['stale'];
            document.querySelectorAll('div[name="mu2e_state_time"]').forEach(div => {
                if (stale_) {
                    if (Number(stale_, 10) > 3) div.innerHTML = "In transition, stale for " + formatTime(Number(stale_, 10));
                    else div.innerHTML = "In transition";
                } else div.innerHTML = "In transition";
            });
        }
    } else {
        reportError("'GatewaySupervisor' not found.");
    }

    // modify the app status table
    //res.forEach(app =>{
    //    console.log(app)
    //});
}

async function updateArtdaq() {
    // only if corresponding div is present
    let div = document.getElementById("mu2e_artdaq");
    if (div == undefined) return;
    div.replaceChildren()

    const title = document.createElement("div")
    title.classList.add("mu2e_title")
    title.style.cssText = "grid-column: 1/6; grid-row: 1";
    title.textContent = "Artdaq"
    //span.appendChild(toggle)
    //title.appendChild(span)
    div.appendChild(title)
    const res = await getDAQState();
    //let connected = getTableDiv(1,2);
    //connected.innerHTML = "TFM: "+(res.connected == "true" ? "connected" : "not connected");
    //setStatusColor(connected,(res.connected  == "true" ? "" : "mu2e_warning"));
    let state = getTableDiv(2, 2);
    state.classList.remove("mu2e_list"); state.classList.add("mu2e_data");
    state.innerHTML = res.state + (res.connected == "true" ? "" : " (not connected)")
    state.setAttribute("name", "mu2e_artdaq_state")
    setStatusColor(state, (res.connected == "true" ? "" : "mu2e_warning"));
    //div.appendChild(connected)
    div.appendChild(state)

    let tfm = getTableDiv(1, 2);
    tfm.innerHTML = "TFM at " + ('host' in res ? res.host : "host") + ":" + ('port' in res ? res.port : "port")
    div.appendChild(tfm)

    // configs
    let config = getTableDiv(3, 2);
    config.innerHTML = "config <span class=\"mu2e_config\" path=\"/ARTDAQPropertyTable\" uid=\"SupervisorTFMConfig\" field=\"PropertyValue\"></span>"
    div.appendChild(config)
    let generate = getTableDiv(4, 2);
    generate.innerHTML = "generate: <span class=\"mu2e_config\" path=\"/ARTDAQPropertyTable\" uid=\"SupervisorTFMGenerateConfig\" field=\"PropertyValue\" onClick=\"showSaveButton()\"></span>"
    div.appendChild(generate)

    let save = getTableDiv(5, 2);
    save.innerHTML = '<button id =\"saveConfig\" style="display:none"; onClick="document.getElementById(\'save_msg\').innerHTML = \'\'; saveConfig(function (msg) {document.getElementById(\'save_msg\').innerHTML += msg+\'<br>\'})">save</button>';
    //save.style.display = "None";
    div.appendChild(save);
    let save_msg = getTableDiv(5, 3);
    save_msg.style.cssText = "grid-column: 2/6; grid-row: 3";
    save_msg.id = "save_msg";
    div.appendChild(save_msg);



    let header1 = getTableDiv(1, 3);
    header1.innerHTML = "process (host:port)";
    let header2 = getTableDiv(2, 3);
    header2.innerHTML = "rank:subsystem";
    let header3 = getTableDiv(2, 3);
    header3.innerHTML = "state";
    //div.appendChild(header1)
    //div.appendChild(header2)
    //div.appendChild(header3)

    if (res.connected) {
        let processes = Object.entries(res).filter(([key, value]) => typeof value === 'object');
        processes.sort(([, a], [, b]) => a.rank - b.rank);
        let rowidx = 4;
        processes.forEach(([proc_name, proc]) => {
            let name = getTableDiv(1, rowidx);
            name.innerHTML = proc_name + ":" + proc.subsystem + " at " + proc.host + ":" + proc.port;//+", "+proc.rank+":"+proc.subsystem;;
            //let info = getTableDiv(2,rowidx);
            //info.innerHTML = proc.rank+": "+proc.subsystem;
            let state = getTableDiv(2, rowidx);
            state.classList.remove("mu2e_list"); state.classList.add("mu2e_data");
            state.innerHTML = proc.state;
            state.setAttribute("name", proc_name + "_state");
            if (proc.state == "Running") {
                setStatusColor(state, "mu2e_ok");
            } else if (proc.state == "Configuring") {
                setStatusColor(state, "mu2e_transition");
            } else {
                setStatusColor(state, "");
            }
            let count = getTableDiv(4, rowidx);
            count.setAttribute("name", "mu2e_dcs");
            let rate = getTableDiv(3, rowidx);
            rate.setAttribute("name", "mu2e_dcs");
            let size = getTableDiv(5, rowidx);
            size.setAttribute("name", "mu2e_dcs");
            if (proc_name.substring(0, 2) === "br") { // Boardreaders are different
                rate.setAttribute("data", "Mu2e:TDAQ_crv:" + proc_name + ":1:Fragment_Rate");
                rate.setAttribute("format", "1")
                rate.setAttribute("units", "ev/s")
                count.setAttribute("data", "Mu2e:TDAQ_crv:" + proc_name + ":1:Fragment_Count");
                count.setAttribute("format", "0")
                count.setAttribute("units", "frags")
                size.setAttribute("data", "Mu2e:TDAQ_crv:" + proc_name + ":1:Average_Fragment_Size");
                size.setAttribute("format", "0")
                count.setAttribute("units", "frags")
            } else {
                rate.setAttribute("data", "Mu2e:TDAQ_crv:" + proc_name + ":Event_Rate");
                rate.setAttribute("format", "1")
                rate.setAttribute("units", "ev/s")
                count.setAttribute("data", "Mu2e:TDAQ_crv:" + proc_name + ":Average_Event_Building_Time");
                count.setAttribute("format", "6")
                count.setAttribute("units", "s")
                size.setAttribute("data", "Mu2e:TDAQ_crv:" + proc_name + ":Shared_Memory_Full_%");
                size.setAttribute("format", "0")
                size.setAttribute("units", "%")
            }
            div.appendChild(name);
            //div.appendChild(info);
            div.appendChild(state);
            div.appendChild(rate);
            div.appendChild(count);
            div.appendChild(size);
            // add statistics
            rowidx++;
        });
        loadDcsChannels();
        loadConfigChannels();
        console.log(mu2e_config_fields);
    }


}

async function handleArtdaq() {
    let div = document.getElementById("mu2e_artdaq");
    if (div == undefined) return;
    const res = await getDAQState();
    let states = document.getElementsByName("mu2e_artdaq_state");
    states.forEach(state => {
        if ('connected' in res) {
            if (res.state == "running:100") {
                state.innerHTML = "Running";
                setStatusColor(state, "mu2e_ok");
            } else {
                state.innerHTML = res.state + (res.connected == "true" ? "" : " (not connected)")
                setStatusColor(state, "");
            }
        } else {
            state.innerHTML = "not connected";
        }
    });
    let processes = Object.entries(res).filter(([key, value]) => typeof value === 'object');
    processes.forEach(([proc_name, proc]) => {
        let states = document.getElementsByName(proc_name + "_state");
        states.forEach(state => {
            state.innerHTML = proc.state;
            if (proc.state == "Running") {
                setStatusColor(state, "mu2e_ok");
            } else if (proc.state == "Configuring") {
                setStatusColor(state, "mu2e_transition");
            } else {
                setStatusColor(state, "");
            }
        });
    });
}

async function handleCurrentState() {
    let res = await getCurrentState();
    let time_ = res['time_in_state'];
    let in_transition_ = res['in_transition'];
    document.querySelectorAll('div[name="mu2e_state_time"]').forEach(div => {
        if (in_transition_ == 0) {
            if (time_) {
                div.innerHTML = "Time in state: " + formatTime(Number(time_, 10));
            } else {
                div.innerHTML = "Time in state: XX:XX:XX";
            }
        } else {
            //div.innerHTML = "In transition", lets handle this by getAppStatus where we also have the stale time
        }
    });
}

function addNav() {
    let nav = document.querySelector("div[id='mu2e_nav']")
    let overview = document.createElement("a")
    overview.href = "Mu2eIndex.html"
    overview.innerHTML = "Overview"
    overview.id = "Index"
    let alarms = document.createElement("a")
    alarms.href = "Mu2eAlarms.html"
    alarms.innerHTML = "Alarms"
    alarms.id = "Alarms"
    let message = document.createElement("a")
    message.href = "Mu2eMessages.html"
    message.innerHTML = "Messages"
    message.id = "Messages"
    let runlog = document.createElement("a")
    runlog.href = "Mu2eRunLog.html"
    runlog.innerHTML = "Run Log"
    runlog.id = "RunLog"
    let config = document.createElement("a")
    config.href = "Mu2eCrvConfig.html"
    config.innerHTML = "Configurations"
    config.id = "Config"

    if (nav) {
        nav.appendChild(overview)
        nav.appendChild(alarms)
        nav.appendChild(message)
        nav.appendChild(runlog)
        nav.appendChild(config)

        nav.appendChild(document.createElement("br"))
    }
}

function updateNav(active) {
    let nav_active = document.querySelector("div[id='mu2e_nav'] a[id='" + active + "']")
    if (nav_active) nav_active.classList.add("mu2e_nav_active")
}

function updateHeader() {
    const header = document.querySelector("div[id='mu2e_header']")
    if (header) {
        header.innerHTML = "mu2e CRV";
        let flaot = document.createElement("span")
        flaot.classList.add("mu2e_right_float")
        let config_name = document.createElement("div");
        config_name.setAttribute("name", "mu2e_config_name");
        config_name.style.display = "inline"
        let config = document.createElement("div");
        config.setAttribute("name", "mu2e_config");
        config.style.display = "inline"
        flaot.appendChild(config_name);
        flaot.innerHTML += ": "
        flaot.appendChild(config);
        header.appendChild(flaot);
    }
}

// uses global mu2e_dcs_channels
async function handleEPICS() {
    let res = await getPvData(mu2e_dcs_channels);
    if (res == undefined) return;
    //console.log(res)
    Object.entries(res).forEach(([pvName, pv]) => {
        document.querySelectorAll('div[data="' + pvName + '"]').forEach(div => {
            let value = pv["Value"];
            if (format = div.getAttribute("format")) {
                if (!isNaN(Number(format))) // number of digis
                    value = Number(value).toFixed(format)
                else if (format == "hex") {
                    value = "0x" + Number(value).toString(16)
                }

            }
            //if(title = div.getAttribute("title"))
            //    value = title+": "+value
            if (units = div.getAttribute("units"))
                value += " " + units
            div.textContent = value;
            if ((pv["Severity"] == "MINOR")) {
                setStatusColor(div, "mu2e_warning");
            } else if ((pv["Severity"] == "MAJOR")) {
                setStatusColor(div, "mu2e_bad");
            } else {
                setStatusColor(div, "");
            }

            const time = new Date(Number(pv["Timestamp"]) * 1000);
            const dtime = Math.floor(Date.now() / 1000) - Number(pv["Timestamp"])

            addTime(div, time, dtime)

            // handle bitfields
            if (bitfield_group = div.getAttribute("bitfield")) {
                document.querySelectorAll('div[name="' + bitfield_group + '"]').forEach(div_ => {
                    const bit = div_.getAttribute("bit")
                    if (bit) {
                        let span = div_.querySelector("span")
                        if (span == undefined) {
                            span = document.createElement("span")
                            div_.appendChild(span)
                        }
                        console.log()
                        span.innerHTML = (parseInt(pv["Value"]) & (1 << bit)) !== 0 ? "[x]" : "[   ]";
                    }

                });
            }
            // handle callback functions
            if (callback = div.getAttribute("callback")) {
                window[callback](div);
            }
        });
    });
}

// uses global mu2e_config_fields
async function handleConfig() {
    for (let table in mu2e_config_fields) {
        const fieldSet = new Set(mu2e_config_fields[table]["fields"])
        const uidSet = new Set(mu2e_config_fields[table]["uids"])
        console.log(table, [...fieldSet].join(","), [...uidSet].join(","))
        getTreeNodeFieldValues(
            table,
            [...fieldSet].join(","),
            [...uidSet].join(",")
        ).then(res => {
            console.log(res)
            mu2e_config_fields[table]["fields"].forEach(field => {
                //for (let field in mu2e_config_fields[table]["fields"]) {
                mu2e_config_fields[table]["uids"].forEach(uid => {
                    //for (let uid in mu2e_config_fields[table]["uids"]) {
                    let span = document.querySelector("span[class='mu2e_config'][path='" + table + "'][uid='" + uid + "'][field='" + field + "']");
                    if (span) {
                        let a = document.createElement("a")
                        a.classList.add("mu2e_editable")
                        a.href = "#"
                        a.addEventListener('click', function (event) { enableEdit(event.target); })
                        let record;
                        if (Array.isArray(res["fieldValues"])) {
                            record = res["fieldValues"].find(obj => uid in obj)[uid];
                        } else { // single UID requested
                            record = res["fieldValues"][uid];
                        }
                        //if(mu2e_config_fields[table]["fields"].length > 1) {
                        if (Array.isArray(record["FieldValue"])) {
                            a.innerHTML = record["FieldValue"][record["FieldPath"].indexOf(field)]
                        } else {
                            if (record["FieldPath"] == field) {
                                a.innerHTML = record["FieldValue"]
                            }
                        }
                        span.innerHTML = ""
                        span.appendChild(a)
                    }
                })
            })
        });
    }
}

function enableEdit(el) {
    const value = el.innerText;
    // check if On/Off, Yes/No field
    let newEl;
    if (["On", "ON", "Off", "OFF", "on", "off", "Yes", "No", "YES", "NO", "yes", "no", "True", "False", "true", "false"].includes(value)) {
        newEl = document.createElement('select');
        op1 = document.createElement('option');
        op2 = document.createElement('option');
        if (["On", "ON", "Off", "OFF", "on", "off"].includes(value)) {
            op1.innerHTML = "On";
            op1.value = "On";
            op2.innerHTML = "Off";
            op2.value = "Off";
            if (["On", "ON", "on"].includes(value)) op1.setAttribute("selected", "selected");
            else op2.setAttribute("selected", "selected");
        } else if (["yes", "YES", "yes", "no", "No", "NO"].includes(value)) {
            op1.innerHTML = "Yes";
            op1.value = "Yes";
            op2.innerHTML = "No";
            op2.value = "No";
            if (["Yes", "YES", "yes"].includes(value)) op1.setAttribute("selected", "selected");
            else op2.setAttribute("selected", "selected");
        } else if (["true", "True", "TRUE", "false", "False", "FALSE"].includes(value)) {
            op1.innerHTML = "True";
            op1.value = "True";
            op2.innerHTML = "False";
            op2.value = "False";
            if (["true", "True", "TRUE"].includes(value)) op1.setAttribute("selected", "selected");
            else op2.setAttribute("selected", "selected");
        }
        newEl.appendChild(op1)
        newEl.appendChild(op2)
    } else {
        newEl = document.createElement('input');
        newEl.type = 'text';
        newEl.value = value;
        newEl.size = (value.length > 5) ? value.length : 5;
    }
    newEl.setAttribute("oldValue", value);
    newEl.addEventListener('focusout', function (event) { disableEditAndSave(event.target); })
    el.parentNode.replaceChild(newEl, el);
    newEl.focus();
}

function disableEdit(el) {
    const value = el.value;
    const a = document.createElement('a');
    a.href = "#"
    a.classList.add("mu2e_editable")
    a.innerHTML = value;
    a.addEventListener('click', function (event) { enableEdit(event.target); })
    el.parentNode.replaceChild(a, el);
}

function disableEditAndSave(el) {
    const oldValue = el.oldValue;
    const value = el.value;
    if (value != oldValue) {
        const configs = el.parentNode;
        const table = configs.getAttribute("path");
        const uid = configs.getAttribute("uid");
        const field = configs.getAttribute("field");
        console.log("SAVE", table, uid, field, value);
        setTreeNodeFieldValues(table, uid, field, value);
    }
    disableEdit(el);
}

// arguments
function toggleView(el, className = "mu2e_collapsable") {
    document.querySelectorAll("div[class*=" + className + "]").forEach(div => {
        if (el.innerHTML == "+") {
            div.style.display = 'grid';
        } else {
            div.style.display = 'none';
        }
    });
    if (el.innerHTML == "+") {
        el.innerHTML = "-";
    } else {
        el.innerHTML = "+";
    }
}


async function updateAlarms() {
    let div = document.querySelector("div[id='mu2e_alarms']")
    if (div == undefined) return;
    let res = await getAlarmChecks();

    let rowindex_start = 3;
    let rowidx = rowindex_start;
    let channels_ = []; // store all channels to load settings afterwards
    res["alarms"].forEach(alarm => {
        let name = document.createElement("div")
        name.style.cssText = "grid-column: 1; grid-row: " + rowidx.toString();
        name.classList.add("mu2e_list")
        name.innerHTML = alarm["name"].substring(14)
        let val = document.createElement("div")
        val.style.cssText = "grid-column: 2; grid-row: " + rowidx.toString();
        val.classList.add("mu2e_list")
        val.setAttribute("name", "mu2e_dcs")
        val.setAttribute("data", alarm["name"])
        val.setAttribute("format", "2")
        div.appendChild(name)
        div.appendChild(val)

        channels_.push(alarm["name"])
        rowidx++;
    });
    loadDcsChannels();

    let settings = await getPVSettings(channels_);
    rowidx = rowindex_start;
    res["alarms"].forEach(alarm => { // secnd loop too load settings
        let alarmName = alarm["name"]
        let hi = document.createElement("div")
        hi.style.cssText = "grid-column: 5; grid-row: " + rowidx.toString();
        hi.classList.add("mu2e_list")
        hi.innerHTML = Number(settings[alarmName]["Upper_Warning_Limit"]).toFixed(2)
        div.appendChild(hi)
        let hihi = document.createElement("div")
        hihi.style.cssText = "grid-column: 6; grid-row: " + rowidx.toString();
        hihi.classList.add("mu2e_list")
        hihi.innerHTML = Number(settings[alarmName]["Upper_Alarm_Limit"]).toFixed(2)
        div.appendChild(hihi)
        let lo = document.createElement("div")
        lo.style.cssText = "grid-column: 4; grid-row: " + rowidx.toString();
        lo.classList.add("mu2e_list")
        lo.innerHTML = Number(settings[alarmName]["Lower_Warning_Limit"]).toFixed(2)
        div.appendChild(lo)
        let lolo = document.createElement("div")
        lolo.style.cssText = "grid-column: 3; grid-row: " + rowidx.toString();
        lolo.classList.add("mu2e_list")
        lolo.innerHTML = Number(settings[alarmName]["Lower_Alarm_Limit"]).toFixed(2)
        div.appendChild(lolo)
        rowidx++;
    });
}

function addMsg(time, msg) {
    let row = document.createElement("div")
    row.classList.add("mu2e_grid")
    row.style.padding = "2px";
    row.setAttribute("name", "msg_row")
    //row.classList.add("mu2e_container")
    let row_time = document.createElement("div")
    row_time.classList.add("mu2e_list")
    row_time.style.cssText = "grid-column: 1; grid-row: 1";
    row_time.innerHTML = time.toLocaleTimeString(['en-GB'], { hour: '2-digit', minute: '2-digit', second: '2-digit' }) + "&nbsp;"
    row.appendChild(row_time)
    let row_msg = document.createElement("div")
    row_msg.classList.add("mu2e_list")
    row_msg.style.cssText = "grid-column: 2/6; grid-row: 1";
    row_msg.setAttribute("name", "msg_msg")
    //row_msg.appendChild(row_time)
    row_msg.innerHTML += msg
    //addTime(row_msg, time)
    row.appendChild(row_msg)
    return row;
}

async function updateMessages() {
    let div = document.querySelector("div[id='mu2e_messages']")
    let div_last = document.querySelector("div[id='mu2e_last_message']")
    if ((div == undefined) && (div_last == undefined)) return;
    let res = await getSystemMessages(history = true);
    for (let i = res["systemMessages"].length - 1; i >= 0; i--) {
        const msg = res["systemMessages"][i];
        //res["systemMessages"].forEach(msg => {
        let time = new Date(Number(msg["time"]) * 1000);
        const row = addMsg(time, msg["message"]);
        if (div) div.appendChild(row)
        if ((i == res["systemMessages"].length - 1) && (div_last)) {
            div_last.innerHTML = ""
            div_last.appendChild(row)
        }
        //});
    }

}
async function handleMessages() {
    let res = await getSystemMessages();
    if (res["systemMessages"]) {
        res["systemMessages"].forEach(msg => {
            let time = new Date(Number(msg["time"]) * 1000);
            const row = addMsg(time, msg["message"]);
            setStatusColor(row, "mu2e_warning");
            let div = document.querySelector("div[id='mu2e_messages']")
            if (div) {
                let div_first = div.querySelector("div[name='msg_row']>div[name='msg_msg']")
                if (div_first == undefined || (div_first.innerHTML != msg["message"])) {
                    //div.appendChild(row);
                    div.insertBefore(row, div.children[1]);
                } else if (div_first) {
                    let div_row = div.querySelector("div[name='msg_row']")
                    setStatusColor(div_row, "")
                }
            }
            let div_last = document.querySelector("div[id='mu2e_last_message']")
            if (div_last) {
                let div_first = div_last.querySelector("div[name='msg_row']>div[name='msg_msg']")
                if (div_first == undefined || (div_first.innerHTML != msg["message"])) {
                    div_last.innerHTML = ""
                    div_last.appendChild(row);
                } else {
                    let div_row = div_last.querySelector("div[name='msg_row']")
                    setStatusColor(div_row, "")
                }
            }
        });
    }
}

async function updateRunInfo() {
    //console.log("updateRunInfo")
    // check if we should do it
    let divs = document.querySelectorAll('div[name="mu2e_run_number"]')
    if (divs) { // only get data if we need it
        let res = await getRunInfo(); // no argument: get current run number
        if ((res != undefined) && ("error" in res["plugin"])) { // handle case where no run is defined
            divs.forEach(div => {
                div.innerHTML = "Run: N/A";//res["plugin"]["error"]
            });
        } else if (res != undefined) {
            const plugin = res["plugin"];
            divs.forEach(div => { // mu2e_run_number
                let a = document.createElement("a")
                a.href = "Mu2eRunLog.html"
                a.innerHTML = plugin["run_number"]
                div.innerHTML = ""
                div.appendChild(a)
                div.innerHTML = "Run: " + div.innerHTML;

                //let time = new Date(plugin["time"])
                //addTime(div, time)
            });
            document.querySelectorAll('div[name="mu2e_run_start"]').forEach(div => {
                let time = new Date(plugin["time"])
                //addTime(div, time)
                div.innerHTML = "Run start: " + time.toLocaleTimeString(['en-GB'], { hour: '2-digit', minute: '2-digit', second: '2-digit' })
            });
            document.querySelectorAll('div[name="mu2e_run_config"]').forEach(div => {
                div.innerHTML = "Run config: " + plugin["configuration"] + " (" + plugin["configuration_version"] + ")";
                //div.innerHTML +=
            });
            //document.querySelectorAll('div[name="mu2e_run_trigger"]').forEach(div => {
            //    div.innerHTML = plugin["configuration"]+"("+plugin["configuration_version"]+")";
            //});
            document.querySelectorAll('div[name="mu2e_run_type"]').forEach(div => {
                div.innerHTML = "Run type: " + plugin["run_type"];//+"("+plugin["host_name"]+" - "+plugin["artdaq_partition"]+")"
                //div.innerHTML += ": "+plugin["artdaq_partition"]
            });
            document.querySelectorAll('div[name="mu2e_run_host"]').forEach(div => {
                div.innerHTML = "Run host: " + plugin["host_name"] + " (" + plugin["artdaq_partition"] + ")"
            });
            document.querySelectorAll('div[name="mu2e_run_transition"]').forEach(div => {
                const transitions = plugin["transitions"];
                console.log(transitions)
                const t_idx = transitions.length - 1;
                if (t_idx >= 0) {
                    const time = new Date(transitions[t_idx]["time"]);
                    const msg = transitions[t_idx]["type"];
                    div.innerHTML = "Last run transition: "
                    div.innerHTML += time.toLocaleTimeString(['en-GB'], { hour: '2-digit', minute: '2-digit', second: '2-digit' })
                    div.innerHTML += " " + msg
                }
            });
            document.querySelectorAll('div[name="mu2e_run_transitions"]').forEach(div => {
                const transitions = plugin["transitions"];
                for (let idx = transitions.length - 1; idx >= 0; idx--) {
                    //transitions.forEach(transition => {
                    const transition = transitions[idx]
                    let line = document.createElement("div")
                    let time = new Date(transition["time"]);
                    line.innerHTML = time.toLocaleTimeString(['en-GB'], { hour: '2-digit', minute: '2-digit', second: '2-digit' })
                    line.innerHTML += " " + transition["type"]
                    div.appendChild(line)
                    //});
                }
            });
        }
    }
}

async function updateConfig() {
    const config_divs = document.querySelectorAll('div[name="mu2e_config"]');
    const config_name_divs = document.querySelectorAll('div[name="mu2e_config_name"]');
    let const_alias = undefined;
    if (config_divs != undefined || config_name_divs != undefined) {
        getActiveTableGroups().then(res => {
            if (config_divs) config_divs.forEach(div => {
                div.innerHTML = res["Configuration-ActiveGroupName"] + "(" + res["Configuration-ActiveGroupKey"] + ")"
            });
            if (config_name_divs) config_name_divs.forEach(div => {
                div.innerHTML = res["Configuration-ActiveGroupAlias"]
            })
            const_alias = res["Configuration-ActiveGroupAlias"];
        });
    }

    // alias list in dropdown, not used at the moment
    const alias_list = document.querySelector('div[id="mu2e_config_list"]');
    if (alias_list) {
        getAliasList().then(res => {
            let sel = document.createElement("select")
            const aliases = res["aliases"];
            for (let alias in aliases) {
                const alias_ = alias.split("_")
                if (alias_[alias_.length - 1] == "config") {
                    const group = aliases[alias]["config_key"].split("_")[0];
                    const key = aliases[alias]["config_key"].split("_")[1].substring(1);
                    let op = document.createElement("option")
                    //op.innerHTML = group+"("+key+")"
                    op.innerHTML = alias;
                    op.value = alias;
                    if (alias == const_alias) {
                        op.selected = "selected"
                    }
                    sel.appendChild(op)
                }
            }
            alias_list.innerHTML = ""
            alias_list.appendChild(sel)
        });
    }

    const configs = document.querySelector('div[id="mu2e_configurations"]');
    //console.log(configs)
    if (configs) {
        getAliasList().then(res => {
            const aliases = res["aliases"];
            let rowid = 2;
            for (let alias in aliases) {
                const alias_ = alias.split("_")
                if (alias_[alias_.length - 1] == "config") {
                    const group = aliases[alias]["config_key"].split("_")[0];
                    const key = aliases[alias]["config_key"].split("_")[1].substring(1);
                    let el = document.createElement("div")
                    el.classList.add("mu2e_list"); el.classList.add("mu2e_collapsable");
                    el.style.cssText = "grid-column: 1; grid-row: " + rowid.toString() + ";"
                    //el.innerHTML = alias;
                    let a = document.createElement("a")
                    a.href = "#"
                    a.innerHTML = alias;
                    a.addEventListener("click", function (group, key) { activateTableGroup(group, key).then(res => { updateConfig(); }) }.bind(null, group, key));
                    el.appendChild(a)
                    configs.appendChild(el);
                    let el2 = document.createElement("div")
                    el2.classList.add("mu2e_list"); el2.classList.add("mu2e_collapsable")
                    el2.style.cssText = "grid-column: 2; grid-row: " + rowid.toString() + ";"
                    let a2 = document.createElement("a")
                    a2.href = "#"
                    a2.innerHTML = group + "(" + key + ")";
                    a2.addEventListener("click", function (group, key) { activateTableGroup(group, key).then(res => { updateConfig(); }) }.bind(null, group, key));
                    //el2.innerHTML = group+"("+key+")";
                    el2.appendChild(a2)
                    configs.appendChild(el2);
                    let el3 = document.createElement("div")
                    el3.classList.add("mu2e_list"); el3.classList.add("mu2e_collapsable")
                    el3.style.cssText = "grid-column: 3; grid-row: " + rowid.toString() + ";"
                    el3.innerHTML = aliases[alias]["config_alias_comment"];
                    configs.appendChild(el3);
                    let el4 = document.createElement("div")
                    el4.classList.add("mu2e_list"); el4.classList.add("mu2e_collapsable")
                    el4.style.cssText = "grid-column: 4; grid-row: " + rowid.toString() + ";"
                    const time = new Date(Number(aliases[alias]["config_create_time"]) * 1000);
                    el4.innerHTML = time.toLocaleTimeString(['en-US'], { year: '2-digit', month: '2-digit', day: '2-digit', hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit' });
                    configs.appendChild(el4);
                    rowid++;
                }
            }
        });
    }


}

async function loadRunLog(n = 10) {
    let div = document.querySelector('div[id="mu2e_runlog"]')
    if (div) {
        let rowid = 3;
        let res = await getRunInfo(-n);
        if ((res != undefined) && ("runs" in res["plugin"])) {
            res["plugin"]["runs"].forEach(run => {
                let run_number = document.createElement("div")
                run_number.style.cssText = "grid-column: 1; grid-row: " + rowid.toString() + ";"
                run_number.classList.add("mu2e_list")
                run_number.innerHTML = run["run_number"]
                div.appendChild(run_number)

                let run_time = document.createElement("div")
                run_time.style.cssText = "grid-column: 2; grid-row: " + rowid.toString() + ";"
                run_time.classList.add("mu2e_list")
                let time = new Date(run["time"])
                run_time.innerHTML = time.toLocaleTimeString(['en-US'], { year: '2-digit', month: '2-digit', day: '2-digit', hour12: false, hour: '2-digit', minute: '2-digit', second: '2-digit' })
                div.appendChild(run_time)

                let run_trans = document.createElement("div")
                run_trans.style.cssText = "grid-column: 3; grid-row: " + rowid.toString() + ";"
                run_trans.classList.add("mu2e_list")
                let trans_time = new Date(run["last_transition_time"])
                run_trans.innerHTML = trans_time.toLocaleTimeString(['en-GB'], { hour: '2-digit', minute: '2-digit', second: '2-digit' })
                run_trans.innerHTML += " " + run["last_transition"];
                div.appendChild(run_trans)

                let run_type = document.createElement("div")
                run_type.style.cssText = "grid-column: 4; grid-row: " + rowid.toString() + ";"
                run_type.classList.add("mu2e_list")
                run_type.innerHTML = run["run_type"] + " - "; //+run["configuration"]+" ("+run["configuration_version"]+")";
                let a = document.createElement("a")
                a.href = "#"
                a.addEventListener("click", function (group, key) { loadConfig(group, key) }.bind(null, run["configuration"], run["configuration_version"]));
                a.innerHTML = run["configuration"] + "(" + run["configuration_version"] + ")";
                run_type.appendChild(a)
                div.appendChild(run_type)

                let run_host = document.createElement("div")
                run_host.style.cssText = "grid-column: 5; grid-row: " + rowid.toString() + ";"
                run_host.classList.add("mu2e_list")
                run_host.innerHTML = run["host_name"] + " (" + run["artdaq_partition"] + ")";
                div.appendChild(run_host)
                rowid++;
            });
        }
    }
}

async function handleAlarms() {
    let res = await getAlarmChecks();
    const nactive = res != undefined ? res["nactive"] : undefined;
    const total = res != undefined ? res["total"] : undefined;
    document.querySelectorAll('div[name="mu2e_alarms"]').forEach(div => {
        let a = div.querySelector("a")
        if (a == undefined) {
            a = document.createElement("a")
            a.href = "Mu2eAlarms.html"
            div.appendChild(a)
        }
        if (nactive != undefined) {
            a.innerHTML = "Alarms: " + nactive.toString() + "/" + total.toString()
            if (nactive > 0) {
                setStatusColor(div, "mu2e_bad")
            } else {
                setStatusColor(div, "")
            }
            const time = new Date(Number(res["last_check"]) * 1000);
            const dtime = Math.floor(Date.now() / 1000) - Number(res["last_check"])
            addTime(div, time, dtime)
        } else {
            a.innerHTML = "Alarms: Off"
            setStatusColor(div, "mu2e_warning")
        }
    });
}

var aliasList = [];
async function updateAliasList() {
    let res = await getAliasList();
    //console.log(res)
    aliasList = Object.keys(res['aliases'])
    let config_ = res['UserLastConfigAlias'];
    document.querySelectorAll('div[name="mu2e_last_config"]').forEach(div => {
        if (config_) {
            //const state = document.querySelector('[name="mu2e_GatewaySupervisor_state"]')
            //if(state) {
            //    console.log(state.innerHTML)
            //    if((state.innerHTML == "Configured") || (state.innerHTML == "Runing") || (state.innerHTML == "Starting")) {
            div.innerHTML = "Configured: " + config_;
            //    } else {
            //        div.innerHTML = "Configured: N/A";
            //    }
            //} else {
            //    div.innerHTML = "Configured: N/A";
            //}
        } else {
            div.innerHTML = "Configured: N/A";
        }
    });
}

function reportError(msg) {
    console.log(msg)
}

function clearMessages() {
    document.querySelector("#messages").innerHTML = ""
}

function addMessage(msg) {
    let message = document.querySelector("#messages")
    const newSpan = document.createElement('span');
    newSpan.innerHTML = msg
    message.appendChild(newSpan);
}


// CRV specific functions
async function getPedestals(dtcName, dtcLink, msg_div = undefined) {
    if ((dtcName != undefined) && (dtcLink != undefined)) {
        let res = await rocWrite(0x1316, 0x100, dtcName, dtcLink);
        console.log(res)
        //let msg_div = document.querySelector("[id='msg']");
        if (msg_div) {
            let time = new Date(res["feMacroExec"]["ROC Write"]["exec_time"])
            msg_div.innerHTML = "took pedestals at " + time.toLocaleTimeString(['en-GB'], { hour: '2-digit', minute: '2-digit', second: '2-digit' })
        }
    }
}

async function setPoolEna(dtcName, dtcLink, value = 1, msg_div = undefined) {
    if ((dtcName != undefined) && (dtcLink != undefined)) {
        let res = rocWrite(0x8107, value, dtcName, dtcLink);
        if (msg_div) {
            msg_div.innerHTML = "enabled ROC pooling ('POOLENA 1')"
        }
    }
}
