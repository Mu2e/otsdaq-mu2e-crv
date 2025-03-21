#ifndef CRV_DQM_STYLE_H
#define CRV_DQM_STYLE_H

#include "TCanvas.h"
#include "TGraph.h"
#include "TH1.h"
#include "TPad.h"
#include "TPaveStats.h"
#include "TROOT.h"
#include "TStyle.h"
#include "TColor.h"

class CrvDQMStyle {
  public:
	static void SetStyle() {
		// Reset style first
		gROOT->SetStyle("Plain");

		// Pad margins
		gStyle->SetPadTopMargin(0.1);
		gStyle->SetPadBottomMargin(0.15);
		gStyle->SetPadLeftMargin(0.15);
		gStyle->SetPadRightMargin(0.05);

		// Stat box
		gStyle->SetOptStat(111111);
		// I cannot reset the size of the stat box for some reason
		// gStyle->SetStatX(0.85);
		// gStyle->SetStatY(0.85);
		// gStyle->SetStatW(0.9);
		// gStyle->SetStatH(0.4);
		gStyle->SetStatBorderSize(1);
		gStyle->SetStatColor(kWhite);
		gStyle->SetStatFont(42);

		// Font settings
		gStyle->SetTextFont(42);
		gStyle->SetLabelFont(42, "xyz");
		gStyle->SetTitleFont(42, "xyz");

		// Text sizes
		gStyle->SetTextSize(0.06);
		gStyle->SetLabelSize(0.06, "xyz");
		gStyle->SetTitleSize(0.06, "xyz");

		// Ticks and divisions
		gStyle->SetPadTickX(1);
		gStyle->SetPadTickY(1);
		gStyle->SetTickLength(0.015);
		gStyle->SetNdivisions(505, "xyz");

		// Frame
		gStyle->SetFrameLineWidth(1);
		gStyle->SetLineWidth(1);

		// Colour palette
		//gStyle->SetPalette(112); // Doesn't work

		// Force style
		gROOT->ForceStyle();
	}

	static void DrawHist(TH1 *hist, const std::string &clr = "blue") {
		if (!hist) {
			return;
		}

		// Basic histogram-specific formatting
		hist->SetLineWidth(2);
		hist->SetFillStyle(1001);
		hist->SetLineStyle(1);
		hist->GetYaxis()->SetTitleOffset(1.1);
		hist->GetXaxis()->SetTitleOffset(1.1);
		hist->SetStats(1); // Ensure stats are on

		// Primary colours
		if (clr == "black") {
			hist->SetLineColor(kBlack);
			hist->SetFillColor(kGray + 1);
		} else if (clr == "blue") {
			// hist->SetLineColor(kAzure + 2);
			// hist->SetFillColor(kAzure - 9);
			hist->SetLineColor(kBlue + 1);
			hist->SetFillColor(kAzure - 4);
		} else if (clr == "green") {
			hist->SetLineColor(kGreen + 2);
			hist->SetFillColor(kGreen - 9);
		} else if (clr == "red") {
			hist->SetLineColor(kRed + 2);
			hist->SetFillColor(kRed - 9);
		}

		hist->Draw("HIST");

		gPad->Update();
		
	}

	static void DrawHist2D(TH2 *hist) {
		if (!hist) {
			return;
		}

		// Basic histogram-specific formatting
		hist->GetYaxis()->SetTitleOffset(1.1);
		hist->GetXaxis()->SetTitleOffset(1.1);
		hist->SetStats(0); // Ensure stats are OFF
		// gStyle->SetPadRightMargin(0.15);
		gPad->SetRightMargin(0.15);

		hist->Draw("COLZ PAL87"); // Have to set palette here, don't know why

		gPad->Update();

	}


	static void DrawGraph(TGraph *graph, const std::string &title, const std::string &clr = "blue") {

		if (!graph) {
			return;
		}

		// Basic graph-specific formatting
		graph->SetLineStyle(1);
		graph->GetYaxis()->SetTitleOffset(1.1);
		graph->GetXaxis()->SetTitleOffset(1.1);
		graph->SetMarkerStyle(20); // full circle
		graph->SetTitle(title.c_str());

		// Primary colours
		if (clr == "black") {
			graph->SetMarkerColor(kBlack);
			graph->SetLineColor(kGray + 1);
		}
		else if (clr == "blue") {
			graph->SetMarkerColor(kBlue + 1);
			graph->SetLineColor(kAzure - 4);
		} else if (clr == "green") {
			graph->SetMarkerColor(kGreen + 2);
			graph->SetLineColor(kGreen - 9);
		} else if (clr == "red") {
			graph->SetMarkerColor(kRed + 2);
			graph->SetLineColor(kRed - 9);
		}

		// Check if graph has points
		if (graph->GetN() > 0) {
			graph->Draw("APL");
		} else {
			// For empty graphs, set some default range and draw without points
			graph->SetPoint(0, 0, 0);
			graph->SetPoint(1, 100, 100);
			graph->Draw("APL"); // Just draw axes and lines, no points
			// Remove the dummy points after drawing
			// graph->Set(0);
		}

		gPad->Update();
	}
};

#endif