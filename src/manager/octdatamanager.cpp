/*
 * Copyright (c) 2018 Kay Gawlik <kaydev@amarunet.de> <kay.gawlik@beuth-hochschule.de> <kay.gawlik@charite.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "octdatamanager.h"

#include <iostream>
#include<filesystem>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/exception/diagnostic_information.hpp>

#include<QString>
#include<QTime>
#include<QMessageBox>
#include<QApplication>
#include<QFileInfo>
#include<QDir>

#include <octdata/datastruct/series.h>
#include <octdata/datastruct/bscan.h>
#include <octdata/datastruct/oct.h>
#include <octdata/octfileread.h>
#include <octdata/filereadoptions.h>
#include <octdata/filewriteoptions.h>

#include <helper/ptreehelper.h>
#include <data_structure/programoptions.h>
#include <data_structure/slobscandistancemap.h>

#include "octmarkerio.h"
#include "octmarkermanager.h"

namespace bpt = boost::property_tree;
namespace bfs = std::filesystem;



OctDataManager::OctDataManager()
: markerstree(std::make_unique<bpt::ptree>())
, markerIO(std::make_unique<OctMarkerIO>(markerstree.get()))
{
	connect(this, &OctDataManager::seriesChanged, this, &OctDataManager::clearSeriesCache);
}



OctDataManager::~OctDataManager() = default;


void OctDataManager::saveMarkersDefault()
{
	if(ProgramOptions::autoSaveOctMarkers())
		triggerSaveMarkersDefault();
}

void OctDataManager::triggerSaveMarkersDefault()
{
	if(!actFilename.isEmpty())
	{
		emit saveMarkerState(actSeries);
		markerIO->saveDefaultMarker(actFilename.toStdString());
		OctMarkerManager::getInstance().resetChangedSinceLastSaveState();
	}
}


void OctDataManagerThread::run()
{
	octData.reset();

	try
	{
		QFileInfo octmarkerPath(QApplication::applicationFilePath());

		OctData::FileReadOptions octOptions;
		octOptions.e2eGray             = static_cast<OctData::FileReadOptions::E2eGrayTransform>(ProgramOptions::e2eGrayTransform());
		octOptions.registerBScanns     = ProgramOptions::registerBScans();
		octOptions.fillEmptyPixelWhite = ProgramOptions::fillEmptyPixelWhite();
		octOptions.holdRawData         = ProgramOptions::holdOCTRawData();
		octOptions.readBScans          = ProgramOptions::readBScans();
		octOptions.rotateSlo           = ProgramOptions::loadRotateSlo();
		octOptions.libPath             = octmarkerPath.dir().absolutePath().toStdString(); // QApplication::applicationFilePath().toStdString();

		OctData::OCT oct = OctData::OctFileRead::openFile(filename.toStdString(), octOptions, this);
		octData = std::make_unique<OctData::OCT>(std::move(oct));
	}
	catch(boost::exception& e)
	{
		error = QString::fromStdString(boost::diagnostic_information(e));
		loadSuccess = false;
		loadError   = true;
	}
	catch(std::exception& e)
	{
		error = QString::fromStdString(e.what());
		loadSuccess = false;
		loadError   = true;
	}
	catch(const char* str)
	{
		error = str;
		loadSuccess = false;
		loadError   = true;
	}
	catch(...)
	{
		error = QString("Unknow error in file %1 line %2").arg(__FILE__).arg(__LINE__);
		loadSuccess = false;
		loadError   = true;
	}
}

bool OctDataManager::checkAndAskSaveBeforContinue()
{
	if(OctMarkerManager::getInstance().hasChangedSinceLastSave()) // TODO: move to new class for handel gui
	{
		QMessageBox::StandardButton result = QMessageBox::warning(nullptr, tr("Unsaved changes"), tr("You have unsaved changes, what will you do?"), QMessageBox::Discard | QMessageBox::Cancel | QMessageBox::Save);
		switch(result)
		{
			case QMessageBox::Cancel:
				return false;
			case QMessageBox::Discard:
				break;
			case QMessageBox::Save:
				triggerSaveMarkersDefault();
				break;
			default:
				qDebug("OctDataManager::openFile: unexpected StandardButton %d", result);
				return false;;
		}
	}
	return true;
}


void OctDataManager::openFile(const QString& filename)
{
	if(loadThread)
		return;

	if(!ProgramOptions::autoSaveOctMarkers())
	{
		if(!checkAndAskSaveBeforContinue())
			return;
	}

	emit loadFileSignal(true);
	try
	{
		saveMarkersDefault();

		loadThread = std::make_unique<OctDataManagerThread>(*this, filename);
		connect(loadThread.get(), &OctDataManagerThread::stepCalulated, this, &OctDataManager::loadOctDataThreadProgress);
		connect(loadThread.get(), &OctDataManagerThread::finished     , this, &OctDataManager::loadOctDataThreadFinish  );
		loadThread->start();
	}
	catch(...)
	{
		emit loadFileSignal(false);
		throw;
	}
}


void OctDataManager::loadOctDataThreadFinish()
{
	emit loadFileSignal(false);

	if(loadThread->success())
	{
		std::unique_ptr octData4Loading = loadThread->getOctData();
		if(octData4Loading->size() == 0)
		{
			QMessageBox msgBox;
			msgBox.setText(tr("No OCT data read from file"));
			msgBox.setIcon(QMessageBox::Critical);
			msgBox.exec();
		}
		else
		{
			QString error;
			try
			{
				markerstree->clear();
				markerIO->loadDefaultMarker(loadThread->getFilename().toStdString());
			}
			catch(boost::exception& e)
			{
				error = QString::fromStdString(boost::diagnostic_information(e));
			}
			catch(std::exception& e)
			{
				error = QString::fromStdString(e.what());
			}
			catch(const char* str)
			{
				error = str;
			}
			catch(...)
			{
				error = QString("Unknow error in file %1 line %2").arg(__FILE__).arg(__LINE__);
			}
			if(!error.isEmpty())
			{
				QMessageBox msgBox;
				msgBox.setText("OctDataManager::openFile: markerload failed: " + error);
				msgBox.setIcon(QMessageBox::Critical);
				msgBox.exec();
			}

			actFilename = loadThread->getFilename();

			octData = std::move(octData4Loading);

			actPatient = octData->begin()->second;
			if(actPatient->size() > 0)
			{
				actStudy = actPatient->begin()->second;

				if(actStudy->size() > 0)
				{
					actSeries = actStudy->begin()->second;
				}
			}

			emit octFileChanged();
			emit octFileChanged(actFilename);
			emit octFileChanged(octData   .get());
			emit patientChanged(actPatient);
			emit studyChanged  (actStudy  );
			emit seriesChanged (actSeries );
			OctMarkerManager::getInstance().resetChangedSinceLastSaveState();
		}
	}
	else
	{
		if(loadThread->hasLoadError())
		{
			const QString& loadError = loadThread->getError();
			QMessageBox msgBox;
			msgBox.setText(tr("error open file: %1").arg(loadThread->getFilename()) + '\n' + loadError);
			msgBox.setIcon(QMessageBox::Critical);
			msgBox.exec();
		}
	}

	loadThread.reset();
}



void OctDataManager::chooseSeries(const std::shared_ptr<const OctData::Series>& seriesReq)
{
	emit saveMarkerState(actSeries);
	
	
	if(seriesReq == actSeries)
		return;
	
	auto [patient, study] = octData->findSeries(seriesReq);

	if(!patient || !study)
		return;

	actPatient = patient;
	emit patientChanged(patient);

	actStudy = study;
	emit studyChanged(study);

	actSeries = seriesReq;
	emit seriesChanged(seriesReq);
}


boost::property_tree::ptree* OctDataManager::getMarkerTreeSeries(const std::shared_ptr<const OctData::Series>& seriesReq)
{
	if(!seriesReq)
		return nullptr;
	
	auto [patient, study] = octData->findSeries(seriesReq);

	return getMarkerTreeSeries(patient, study, seriesReq);
}

boost::property_tree::ptree* OctDataManager::getMarkerTreeSeries(const std::shared_ptr<const OctData::Patient>& patient, const std::shared_ptr<const OctData::Study>& study, const std::shared_ptr<const OctData::Series>& series)
{
	if(!patient || !study || !series)
		return nullptr;

	bpt::ptree& patNode    = PTreeHelper::getNodeWithId(*markerstree, "Patient", patient->getInternalId());
	bpt::ptree& studyNode  = PTreeHelper::getNodeWithId(patNode     , "Study"  , study  ->getInternalId());
	bpt::ptree& seriesNode = PTreeHelper::getNodeWithId(studyNode   , "Series" , series ->getInternalId());


	PTreeHelper::putNotEmpty(patNode   , "PatientUID", patient->getPatientUID());
	PTreeHelper::putNotEmpty(studyNode , "StudyUID"  , study  ->getStudyUID  ());
	PTreeHelper::putNotEmpty(seriesNode, "SeriesUID" , series ->getSeriesUID ());


	return &seriesNode;
}


bool OctDataManager::loadMarkers(QString filename, OctMarkerFileformat format)
{
	markerstree->clear();
	markerIO->loadMarkers(filename.toStdString(), format);
	emit loadMarkerStateAll();
	OctMarkerManager::getInstance().resetChangedSinceLastSaveState();
	return true;
}

void OctDataManager::saveMarkers(QString filename, OctMarkerFileformat format)
{
	emit saveMarkerState(actSeries);
	markerIO->saveMarkers(filename.toStdString(), format);
	OctMarkerManager::getInstance().resetChangedSinceLastSaveState();
}

void OctDataManager::saveOctScan(QString filename)
{
	if(octData)
	{
		OctData::FileWriteOptions fwo;
		fwo.octBinFlat = ProgramOptions::saveOctBinFlat();

		OctData::OctFileRead::writeFile(filename.toStdString(), *octData, fwo);
	}
}

void OctDataManager::saveOctSerie(QString filename)
{
	if(octData && actPatient && actStudy && actSeries)
	{
		OctData::FileWriteOptions fwo;
		fwo.octBinFlat = ProgramOptions::saveOctBinFlat();
		
		OctData::OCT oct;
		OctData::Patient& pat = oct.getInsertId(actPatient->getInternalId());
		// pat.setPatientData(actPatient->getPatientData());
		
		OctData::Study& study = pat.getInsertId(actStudy->getInternalId());
		
		// study.copySeries(*actStudy, actSeries->getInternalId());
		

		OctData::OctFileRead::writeFile(filename.toStdString(), oct, fwo);
	}
}

const SloBScanDistanceMap* OctDataManager::getSeriesSLODistanceMap() const
{
	if(!seriesSLODistanceMap)
	{
		seriesSLODistanceMap = std::make_unique<SloBScanDistanceMap>();
		seriesSLODistanceMap->createData(actSeries.get());
	}

	return seriesSLODistanceMap.get();
}

void OctDataManager::clearSeriesCache()
{
	seriesSLODistanceMap.reset();
}

void OctDataManager::abortLoadingOctFile()
{
	if(loadThread)
		loadThread->breakLoad();
}

OctDataManagerThread::OctDataManagerThread(OctDataManager& dataManager, const QString& filename) 
    : octDataManager(dataManager)
    , filename(filename)
{}

OctDataManagerThread::~OctDataManagerThread() = default;

std::unique_ptr<OctData::OCT> OctDataManagerThread::getOctData()
{
	return std::move(octData);
}
