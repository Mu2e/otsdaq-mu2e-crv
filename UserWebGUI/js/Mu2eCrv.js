// Mu2eCrv.js - CRV-specific model for Mu2eCrv.html
//
//	Builds the ROC/port picture from the configuration tree and dispatches the
//	ROCCosmicRayVetoInterface macros through the parent DTC's
//	"ROC FEMacro - ..." wrappers. Generic pieces live in Mu2eHardware.js
//	(runMacro, runMacroOnUIDs, buildROCMacroInputs, fetchChannels,
//	fetchROCLinkIDs, findOutput, parseNumber).
//
//	Hardware is touched only inside Mu2eCrv.runOnROC(), which the page calls
//	only from button handlers. Everything else is configuration-database
//	traffic or string handling.

var Mu2eCrv = Mu2eCrv || {};

(function () {
	"use strict";

	Mu2eCrv.NUM_PORTS = 24;
	Mu2eCrv.PREFIX = "ROC FEMacro - ";

	// Short names of the ROC macros the page uses; the DTC exposes each as
	// PREFIX + name. Input names are matched by prefix in buildROCMacroInputs.
	Mu2eCrv.MACROS = {
		configure:  "FEB II Configure from Tables",
		pllReset:   "FEB II PLL Reset and Align",
		testLinks:  "Test ROC Links",
		status:     "Get Status",
		alignScore: "FEB II Get Align Score",
		baselines:  "FEB II Baselines",
		led:        "FEB II LED setting",
		threshold:  "FEB II Set Threshold",
		trim:       "FEB II Set Bias Trim",
		bias:       "FEB II Set Bias",
		gateOn:     "FEB II Set Gate OnSpill",
		gateOff:    "FEB II Set Gate OffSpill",
	};

	var _model = null;
	// _model = { dtc, rocs: [{ uid, link, enabled, ports: {1: port, ...}, configError }] }
	// port   = { port, febUID, on, bias[], trim[], threshold[],
	//            gates: {onStart, onEnd, offStart, offEnd} }

	Mu2eCrv.getModel = function () { return _model; };

	// =========================================================================
	// Discovery
	// =========================================================================

	Mu2eCrv.fullName = function (shortName) { return Mu2eCrv.PREFIX + shortName; };

	// A DTC is a CRV DTC if it carries the CRV configure macro. The DTC only
	// registers its ROC macros in configure(), so on a Halted system the macro
	// list is empty; then fall back to the configuration structure and pick
	// DTCs whose UID or ROC UIDs look like CRV.
	Mu2eCrv.findDTCs = function () {
		var map = Mu2eHardware.getFeToMacrosMap() || {};
		var needle = Mu2eCrv.fullName(Mu2eCrv.MACROS.configure);
		var found = [];
		var uids = Object.keys(map);
		for (var i = 0; i < uids.length; ++i)
			if (map[uids[i]][needle]) found.push(uids[i]);
		if (found.length) return found;

		var structure = Mu2eHardware.getStructure();
		if (!structure || !structure.apps) return found;
		for (var a = 0; a < structure.apps.length; ++a) {
			var dtcs = structure.apps[a].dtcs || [];
			for (var d = 0; d < dtcs.length; ++d) {
				var looksCRV = /crv/i.test(dtcs[d].name);
				for (var r = 0; !looksCRV && r < (dtcs[d].rocs || []).length; ++r)
					looksCRV = /crv/i.test(dtcs[d].rocs[r].name);
				if (looksCRV) found.push(dtcs[d].name);
			}
		}
		return found;
	};

	// True once the DTC has registered its ROC macros (system Configured or later).
	Mu2eCrv.macrosAvailable = function () {
		return !!Mu2eCrv.getMacro(Mu2eCrv.MACROS.configure);
	};

	Mu2eCrv.getMacro = function (shortName) {
		if (!_model) return null;
		var macros = Mu2eHardware.getMacrosForDevice(_model.dtc) || {};
		return macros[Mu2eCrv.fullName(shortName)] || null;
	};

	Mu2eCrv.listROCMacroNames = function () {
		if (!_model) return [];
		var macros = Mu2eHardware.getMacrosForDevice(_model.dtc) || {};
		var names = [];
		var keys = Object.keys(macros);
		for (var i = 0; i < keys.length; ++i)
			if (keys[i].indexOf(Mu2eCrv.PREFIX) === 0)
				names.push(keys[i].substring(Mu2eCrv.PREFIX.length));
		names.sort();
		return names;
	};

	// =========================================================================
	// runOnROC - the only hardware path
	//
	//   roc:       model roc ({uid, link})
	//   shortName: key of Mu2eCrv.MACROS, or a literal short macro name
	//   port:      1..24, -1 for all active, undefined to leave unset
	//   extra:     { "input name prefix": value }
	// =========================================================================

	Mu2eCrv.runOnROC = function (roc, shortName, port, extra, callback, onProgress) {
		var name = Mu2eCrv.MACROS[shortName] || shortName;
		var macroObj = Mu2eCrv.getMacro(name);
		if (!macroObj) {
			if (callback) callback({ error: "Macro '" + name + "' not found on " + _model.dtc });
			return;
		}
		var inputs = Mu2eHardware.buildROCMacroInputs(macroObj, roc.link, port, extra);
		Mu2eHardware.runMacro(_model.dtc, Mu2eCrv.fullName(name), inputs,
			callback, onProgress);
	};

	// One request per ROC, all in flight together.
	//   onEach(roc, result), onDone([{roc, result}, ...])
	Mu2eCrv.runOnROCs = function (rocs, shortName, port, extra, onEach, onDone) {
		var remaining = rocs.length;
		var results = [];
		if (!remaining) {
			if (onDone) onDone(results);
			return;
		}
		rocs.forEach(function (roc) {
			Mu2eCrv.runOnROC(roc, shortName, port, extra, function (result) {
				results.push({ roc: roc, result: result });
				if (onEach) onEach(roc, result);
				if (--remaining === 0 && onDone) onDone(results);
			});
		});
	};

	// Run one macro on several ports of ONE ROC, one port after the other. The
	// ROC has a single active-port register, so parallel per-port calls on the
	// same ROC would race. Different ROCs can run in parallel.
	//   onEach(port, result), onDone()
	Mu2eCrv.runOnROCPorts = function (roc, shortName, ports, extra, onEach, onDone) {
		var i = 0;
		function next() {
			if (i >= ports.length) {
				if (onDone) onDone();
				return;
			}
			var port = ports[i++];
			Mu2eCrv.runOnROC(roc, shortName, port, extra, function (result) {
				if (onEach) onEach(port, result);
				next();
			});
		}
		next();
	};

	// =========================================================================
	// Response parsers
	// =========================================================================

	// FEB II Baselines: "response" is 64 comma-separated ADC values,
	// FPGA-major (fpga 0 ch 0..15, fpga 1 ch 0..15, ...). Returns int[64] or null.
	Mu2eCrv.parseBaselines = function (result) {
		if (!result || result.error) return null;
		var nums = String(Mu2eHardware.findOutput(result, "response") || "").match(/-?\d+/g);
		if (!nums || nums.length < 64) return null;
		return nums.slice(0, 64).map(function (n) { return parseInt(n, 10); });
	};

	Mu2eCrv.stats = function (arr) {
		var min = arr[0], max = arr[0], sum = 0;
		for (var i = 0; i < arr.length; ++i) {
			if (arr[i] < min) min = arr[i];
			if (arr[i] > max) max = arr[i];
			sum += arr[i];
		}
		return { min: min, max: max, mean: sum / arr.length };
	};

	// 64 values as 4 lines of 16, one per FPGA
	Mu2eCrv.gridText = function (arr) {
		var lines = [];
		for (var f = 0; f < 4; ++f)
			lines.push("fpga " + f + ": " + arr.slice(f * 16, f * 16 + 16).join(" "));
		return lines.join("\n");
	};

	// Ports of a ROC that are configured on, ascending
	Mu2eCrv.onPorts = function (roc) {
		return Object.keys(roc.ports)
			.map(function (p) { return parseInt(p, 10); })
			.filter(function (p) { return roc.ports[p].on; })
			.sort(function (a, b) { return a - b; });
	};

	Mu2eCrv.parseStatus = function (result) {
		if (!result || result.error) return null;
		var out = Mu2eHardware.findOutput;
		var num = Mu2eHardware.parseNumber;
		return {
			version:    num(out(result, "version")),
			pllLock:    num(out(result, "PLL lock")),
			activeMask: num(out(result, "Active Ports")),
			uptime:     num(out(result, "Uptime")),
			lossErrors: num(out(result, "Link Errors Loss")),
			crcErrors:  num(out(result, "Link Errors CRC")),
			// ROC register 0x41 (markers) and 0x42 (heartbeats), low/high byte
			markerDecoded: num(out(result, "Marker Decoded Cnt")),
			markerDelayed: num(out(result, "Marker Delayed Cnt")),
			heartbeatRx:   num(out(result, "Heartbeat Rx Cnt")),
			heartbeatTx:   num(out(result, "Heartbeat Tx Cnt")),
			drCnt:         num(out(result, "DR Cnt")),
			testCnt:       num(out(result, "Test Cnt")),
			// present only once the FE is rebuilt with the extended Get Status
			lastEWT:       num(out(result, "Last EWT")),
			hbEmpty:       num(out(result, "HB Buffer Empty")),
			hbWords:       num(out(result, "HB Buffer Words")),
			drEmpty:       num(out(result, "DR Buffer Empty")),
			drWords:       num(out(result, "DR Buffer Words")),
			linkWords:     [num(out(result, "Link 0 Word Cnt")),
			                num(out(result, "Link 1 Word Cnt")),
			                num(out(result, "Link 2 Word Cnt"))],
			evEmpty:       num(out(result, "Event Buffer Empty")),
			evFull:        num(out(result, "Event Buffer Full")),
		};
	};

	// Test ROC Links: "result" is "true"/"false"; "response" has lines like
	//   "  port 3: 0x1a2b"  or  "  port 3: read failed after 0.5s retry"
	Mu2eCrv.parseLinkTest = function (result) {
		if (!result || result.error) return null;
		var out = { pass: null, ports: {} };
		var flag = Mu2eHardware.findOutput(result, "result");
		if (flag !== null) out.pass = /true/i.test(String(flag));
		var lines = String(Mu2eHardware.findOutput(result, "response") || "").split(/\r?\n/);
		for (var i = 0; i < lines.length; ++i) {
			var m = lines[i].match(/port\s+(\d+):\s*(.*)$/i);
			if (m) out.ports[parseInt(m[1], 10)] = /fail/i.test(m[2]) ? "fail" : "ok";
		}
		return out;
	};

	// "Port 3: AlignScore = 0x1f" lines -> {3: 31}
	Mu2eCrv.parseAlignScores = function (result) {
		if (!result || result.error) return null;
		var scores = {};
		var lines = String(Mu2eHardware.findOutput(result, "response") || "").split(/\r?\n/);
		for (var i = 0; i < lines.length; ++i) {
			var m = lines[i].match(/port\s+(\d+):\s*AlignScore\s*=\s*(0x[0-9a-f]+|\d+)/i);
			if (m) scores[parseInt(m[1], 10)] = Mu2eHardware.parseNumber(m[2]);
		}
		return scores;
	};

	Mu2eCrv.isPortActive = function (mask, port) {
		if (mask === null || mask === undefined) return null;
		return ((mask >>> (port - 1)) & 1) === 1;
	};

	// =========================================================================
	// Configuration-tree reads (no hardware)
	//
	//   loadConfig(dtcUID, rocs, callback(model))
	//     rocs: [{name, enabled}] as from Mu2eHardware.getROCsForDTC()
	// =========================================================================

	Mu2eCrv.loadConfig = function (dtcUID, rocs, callback) {
		_model = { dtc: dtcUID, rocs: [] };
		var rocUIDs = [];
		for (var i = 0; i < rocs.length; ++i) {
			_model.rocs.push({
				uid: rocs[i].name, enabled: rocs[i].enabled,
				link: -1, ports: {}, configError: null,
			});
			rocUIDs.push(rocs[i].name);
		}
		if (!rocUIDs.length) {
			if (callback) callback(_model);
			return;
		}

		Mu2eHardware.fetchROCLinkIDs(rocUIDs, function (linkMap) {
			for (var r = 0; r < _model.rocs.length; ++r) {
				var roc = _model.rocs[r];
				if (linkMap[roc.uid] !== undefined) roc.link = linkMap[roc.uid];
				else roc.configError = "linkID not found in ROCInterfaceTable.";
			}
			_loadFEBs(0, callback);
		});
	};

	function _loadFEBs(index, callback) {
		if (index >= _model.rocs.length) {
			_model.rocs.sort(function (a, b) { return a.link - b.link; });
			if (callback) callback(_model);
			return;
		}
		var roc = _model.rocs[index];
		Mu2eHardware.fetchChannels(roc.uid, function (chan) {
			if (!chan || chan.tableName !== "SubsystemCRVFebTable") {
				roc.configError = (roc.configError ? roc.configError + " " : "") +
					"No SubsystemCRVFebTable records linked.";
			} else {
				for (var i = 0; i < chan.channels.length; ++i)
					_addPort(roc, chan.channels[i]);
			}
			_loadFEBs(index + 1, callback);
		});
	}

	function _field(fields, name) {
		var keys = Object.keys(fields);
		for (var i = 0; i < keys.length; ++i) {
			var last = keys[i].substring(keys[i].lastIndexOf("/") + 1);
			if (last === name) return fields[keys[i]];
		}
		return undefined;
	}

	function _bitmap(str) {
		var nums = String(str || "").match(/-?\d+/g) || [];
		return nums.map(function (n) { return parseInt(n, 10); });
	}

	function _addPort(roc, channel) {
		var f = channel.fields || {};
		var port = parseInt(_field(f, "Port"), 10);
		if (!(port >= 1 && port <= Mu2eCrv.NUM_PORTS)) return;
		roc.ports[port] = {
			port: port,
			febUID: channel.uid,
			on: /^(1|on|true|yes)$/i.test(String(_field(f, "Status") || "").trim()),
			bias: _bitmap(_field(f, "Bias")),
			trim: _bitmap(_field(f, "Trim")),
			threshold: _bitmap(_field(f, "Threshold")),
			gates: {
				onStart: _field(f, "OnSpillStart"),
				onEnd: _field(f, "OnSpillEnd"),
				offStart: _field(f, "OffSpillStart"),
				offEnd: _field(f, "OffSpillEnd"),
			},
		};
	}

	// =========================================================================
	// Formatting helpers
	// =========================================================================

	Mu2eCrv.range = function (arr) {
		if (!arr || !arr.length) return "";
		var min = Math.min.apply(null, arr);
		var max = Math.max.apply(null, arr);
		return min === max ? String(min) : min + " .. " + max;
	};

	Mu2eCrv.countOn = function (roc) {
		return Object.keys(roc.ports).filter(function (p) { return roc.ports[p].on; }).length;
	};

	// Gate settings of the first enabled FEB - used to prefill the gate inputs.
	Mu2eCrv.firstGates = function () {
		if (!_model) return null;
		for (var r = 0; r < _model.rocs.length; ++r) {
			var ports = Object.keys(_model.rocs[r].ports);
			for (var i = 0; i < ports.length; ++i) {
				var p = _model.rocs[r].ports[ports[i]];
				if (p.on && p.gates.onEnd !== undefined) return p.gates;
			}
		}
		return null;
	};

})();
