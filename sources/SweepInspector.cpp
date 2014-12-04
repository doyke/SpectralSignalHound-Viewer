/*
 * Copyright (c) 2014, Nick Potts
 * All rights reserved.
 *
 *  This Source Code Form is subject to the terms of the 
 *  Mozilla Public License, v. 2.0. If a copy of the MPL 
 *  was not distributed with this file, You can obtain 
 *  one at http://mozilla.org/MPL/2.0/.
*/

#include "SweepInspector.h"

FreqScaleDraw::FreqScaleDraw() : QwtScaleDraw() {}
FreqScaleDraw::~FreqScaleDraw() {}
QwtText FreqScaleDraw::label(double v) const {
  //convert 1320 to 1.3KHz, 15012402 to 15.0MHz ext
  QString rtn;
  switch ( (int) std::log10(v)) {
    case 9: rtn = QString("%1 GHz").arg(v/1e9, 0, 'f', 2); break;
    case 8:
    case 7:
    case 6: rtn = QString("%1 MHz").arg(v/1e6, 0, 'f', 2); break;
    case 5:
    case 4:
    case 3: rtn = QString("%1 kHz").arg(v/1e3, 0, 'f', 2); break;
    case 2:
    case 1:
    case 0: rtn = QString("%1 Hz").arg(v, 0, 'f', 2); break;
  }
  return QwtText(rtn);
}

SweepInspector::~SweepInspector() {
  /** destroy stuffs  */
  delete(d_curve);
  delete(canvas);
  delete(zoomer);
  delete(panner);
  delete(picker);
  delete(grid);
  delete(minfo);
}
SweepInspector::SweepInspector(QWidget *parent) : QWidget(parent), data(NULL), d_curve(NULL) {
  setupUi(this);
  plot->setObjectName( "SweepData" );
  plot->setTitle( "RF Sweep" );
  plot->setAxisTitle( QwtPlot::xBottom, "Frequency");
  plot->setAxisTitle( QwtPlot::yLeft, QString( "Power Level (dBm)"));
  plot->setAutoReplot(true);

  plot->enableAxis(QwtPlot::xBottom, true);
  plot->enableAxis(QwtPlot::yLeft, true);
  plot->enableAxis(QwtPlot::xTop, false);
  plot->enableAxis(QwtPlot::yRight, false);

  canvas = new QwtPlotCanvas();
  canvas->setPalette( Qt::black );
  canvas->setBorderRadius(0);
  plot->setCanvas(canvas);

  //Allow zooming / panning
  zoomer = new QwtPlotZoomer( canvas );
  zoomer->setRubberBandPen( QColor( Qt::white ) );
  zoomer->setTrackerPen( QColor( Qt::white ) );
  panner = new QwtPlotPanner( canvas );
  panner->setMouseButton( Qt::MidButton );

  //Show the X/Y markers that follow the mouse
  picker = new QwtPlotPicker( QwtPlot::xBottom, QwtPlot::yLeft, QwtPlotPicker::CrossRubberBand, QwtPicker::AlwaysOn, canvas);
  picker->setStateMachine( new QwtPickerTrackerMachine() );
  picker->setRubberBandPen( QColor( Qt::cyan ) );
  picker->setRubberBand( QwtPicker::CrossRubberBand );
  picker->setTrackerPen( QColor( Qt::cyan ) );

  //Setup grid
  grid = new QwtPlotGrid();
  grid->enableXMin( true );
  grid->enableYMin( true );
  QColor color(Qt::gray); color.setAlpha(128);
  grid->setMajorPen( color, 1, Qt::DotLine );
  grid->setMinorPen( color, 1, Qt::DotLine );
  grid->attach( plot );

  //format in kHz, MHz, GHz, not raw values
  plot->setAxisScaleDraw(QwtPlot::xBottom, new FreqScaleDraw);

  //connects
  connect(timeIndex, SIGNAL(valueChanged(int)), this, SLOT(loadSweep(int)));
  connect(timeIndex, SIGNAL(sliderMoved(int)), this, SLOT(sliderMoved(int)));
}
void SweepInspector::setpData(QHoundData *newd) {
  data = newd;
  //setup the bar
  timeIndex->setMaximum(data->getNumSweeps()-1);
  timeIndex->setValue(0);
}
void SweepInspector::sliderMoved(int index) {
  //update text despite not letting go
  if (data == NULL) 
    timestamp->setText("");
  else
    timestamp->setText(data->timestampFromIndex(index));
}
void SweepInspector::loadSweep(int index) {
  //load in new sweep values from data
  if (data == NULL) return;
  sliderMoved(index);

  //remove old data and get new
  if (d_curve) d_curve->attach(NULL);
  plot->detachItems();
  fsweep sweep = data->getSweep(index);

  d_curve = new QwtPlotCurve( timestamp->text() ); //Qwt will delete() this when its done with it
  d_curve->setRenderHint( QwtPlotItem::RenderAntialiased );
  d_curve->setStyle( QwtPlotCurve::Lines );
  d_curve->setPen( QColor( Qt::yellow ), 2, Qt::SolidLine );
  d_curve->setSamples( sweep );
  d_curve->attach(plot);

  range freqs = data->limits(FREQ);
  plot->setAxisScale(QwtPlot::xBottom, freqs.first, freqs.second, (freqs.second - freqs.first)/ 5.0);
  plot->setAxisScale(QwtPlot::yLeft, -135, 20, 10.0);
  plot->setTitle( QString("RF Sweep @ %1").arg(timestamp->text()) );
  //set maximum zoom out
  zoomer->setZoomBase(QRectF(QPointF(freqs.first, 20), QPointF(freqs.second, -135)));
  zoomer->zoomBase();

  //find max, min, and average values and drop it on plot as well
  double max = sweep.first().y(), min = sweep.first().y(), avg=0;
  for(int i=0; i < sweep.size(); i++) {
    max = std::max(max, sweep.at(i).y());
    min = std::min(min, sweep.at(i).y());
    avg += sweep.at(i).y();
  } avg /= sweep.size();

  //add markers onto the plot
  QwtText tinfo = QwtText(QString("Max: %1 dBm\tMin: %2 dBm\tAvg: %3 dBm").arg(max).arg(min).arg(avg));
  tinfo.setFont( QFont( "Helvetica", 10, QFont::Bold ) );
  tinfo.setColor( Qt::green );
  tinfo.setRenderFlags(Qt::AlignBottom | Qt::AlignCenter);
  minfo = new QwtPlotMarker(tinfo); 
  minfo->attach(plot);
  minfo->setLabel(tinfo);
  minfo->setValue(freqs.first + (freqs.second-freqs.first)/2, -135); 
  plot->replot();
  plot->repaint();
}
void SweepInspector::save(QString filename) {
  QwtPlotRenderer renderer;
  renderer.exportTo( plot, filename);
}
