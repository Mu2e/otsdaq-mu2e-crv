// ROOT styling for CRV DQM
// Samuel Grant 2025

#ifndef CRV_DQM_STYLE_H
#define CRV_DQM_STYLE_H

#include "TCanvas.h"
#include "TGraph.h"
#include "TH1.h"
#include "TPad.h"
#include "TROOT.h"
#include "TStyle.h"

class CrvDQMStyle
{
  public:
	static void SetStyle()
	{
		TStyle* style = new TStyle("CrvStyle", "CRV DQM styling");

		// Basic canvas settings
		style->SetCanvasColor(kWhite);
		style->SetCanvasBorderMode(0);
		style->SetPadColor(kWhite);
		style->SetPadBorderMode(0);
		style->SetPadTopMargin(0.05);
		style->SetPadBottomMargin(0.13);
		style->SetPadLeftMargin(0.15);
		style->SetPadRightMargin(0.05);

		// Font settings
		style->SetTextFont(42);
		style->SetLabelFont(42, "xyz");
		style->SetTitleFont(42, "xyz");

		// Text sizes
		style->SetTextSize(0.045);
		style->SetLabelSize(0.045, "xyz");
		style->SetTitleSize(0.05, "xyz");

		// Title positioning
		style->SetTitleOffset(1.2, "y");
		style->SetTitleOffset(1.0, "x");

		// Canvas fill
		style->SetFillColor(kWhite);
		style->SetFillStyle(1001);

		// Ticks and divisions
		style->SetPadTickX(1);
		style->SetPadTickY(1);
		style->SetTickLength(0.015);
		style->SetNdivisions(505, "xyz");

		// Frame
		style->SetFrameLineWidth(1);
		style->SetLineWidth(1);

		style->SetOptStat(111111);

		// Use current style - this works in 6.30
		gROOT->SetStyle("CrvStyle");
		gROOT->ForceStyle();
	}

	static void FormatHist(TH1* hist, const std::string& colour = "red")
	{
		if(!hist)
			return;
		// Basic histogram-specific formatting
		// hist->SetStats(0);
		hist->SetLineWidth(2);
		hist->SetFillStyle(1001);
		hist->SetLineStyle(1);
		hist->GetYaxis()->SetTitleOffset(1.6);
		hist->GetXaxis()->SetTitleOffset(1.2);

		// Primary colours
		if(colour == "blue")
		{
			hist->SetLineColor(kAzure + 2);
			hist->SetFillColor(kAzure - 9);
		}
		else if(colour == "green")
		{
			hist->SetLineColor(kGreen + 2);
			hist->SetFillColor(kGreen - 9);
		}
		else if(colour == "red")
		{
			hist->SetLineColor(kRed + 2);
			hist->SetFillColor(kRed - 9);
		}
	}

	static void FormatGraph(TGraph* graph, const std::string& colour = "red")
	{
		if(!graph)
			return;
		// Basic graph-specific formatting
		graph->SetLineStyle(1);
		graph->GetYaxis()->SetTitleOffset(1.6);
		graph->GetXaxis()->SetTitleOffset(1.2);
		graph->SetMarkerStyle(20);  // full circle

		// Primary colours
		if(colour == "blue")
		{
			graph->SetMarkerColor(kAzure + 2);
			graph->SetLineColor(kAzure - 9);
		}
		else if(colour == "green")
		{
			graph->SetMarkerColor(kGreen + 2);
			graph->SetLineColor(kGreen - 9);
		}
		else if(colour == "red")
		{
			graph->SetMarkerColor(kRed + 2);
			graph->SetLineColor(kRed - 9);
		}
	}
};

#endif