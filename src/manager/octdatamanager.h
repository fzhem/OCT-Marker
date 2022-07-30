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

#pragma once

#include <QObject>

#include <QThread>

#include <vector>
#include <string>


#include <boost/property_tree/ptree_fwd.hpp>

#include <globaldefinitions.h>

#include <oct_cpp_framework/callback.h>


class QString;
class OctMarkerIO;
class SloBScanDistanceMap;

namespace OctData
{
	class OCT;
	class Patient;
	class Study;
	class Series;
}

class OctDataManagerThread;

/**
 * @ingroup Manager
 * @brief
 *
 */
class OctDataManager : public QObject
{
	friend class OctDataManagerThread;

	Q_OBJECT
public:
	
	~OctDataManager() override;
	
	static OctDataManager& getInstance()                                      { static OctDataManager instance; return instance; }
	                                                                          
	const QString& getLoadedFilename() const                                  { return actFilename; }
	const std::shared_ptr<const OctData::Patient>& getPatient() const         { return actPatient ; }
	const std::shared_ptr<const OctData::Study  >& getStudy  () const         { return actStudy   ; }
	const std::shared_ptr<const OctData::Series >& getSeries () const         { return actSeries  ; }
	boost::property_tree::ptree* getMarkerTree(const std::shared_ptr<const OctData::Series>& series) { return getMarkerTreeSeries(series); }
	
	void saveMarkersDefault();
	bool checkAndAskSaveBeforContinue();

private slots:
	void loadOctDataThreadProgress(double frac)                     { emit(loadFileProgress(frac)); }
	void loadOctDataThreadFinish();
	void clearSeriesCache();

public slots:
	void openFile(const QString& filename);
	
	void chooseSeries(const std::shared_ptr<const OctData::Series>& seriesReq);


	const SloBScanDistanceMap* getSeriesSLODistanceMap() const;
	
	
	virtual bool loadMarkers(QString filename, OctMarkerFileformat format);
	virtual void saveMarkers(QString filename, OctMarkerFileformat format);
	
	void triggerSaveMarkersDefault();

	void saveOctScan(QString filename);
	void saveOctSerie(QString filename);

	void abortLoadingOctFile();

signals:
	void octFileChanged();
	void octFileChanged(QString filename);
	void octFileChanged(const OctData::OCT*    );
	void patientChanged(const std::shared_ptr<const OctData::Patient>&);
	void studyChanged  (const std::shared_ptr<const OctData::Study  >&);
	void seriesChanged (const std::shared_ptr<const OctData::Series >&);

	void saveMarkerState(const std::shared_ptr<const OctData::Series>&);
	void loadMarkerState(const std::shared_ptr<const OctData::Series>&);

	void loadMarkerStateAll();

	void loadFileSignal(bool loading);
	void loadFileProgress(double frac);


private:
	
	const std::unique_ptr<boost::property_tree::ptree> markerstree;
	const std::unique_ptr<OctMarkerIO                > markerIO   ;

	QString actFilename;
	
	std::unique_ptr<OctData::OCT>           octData   ;
	std::shared_ptr<const OctData::Patient> actPatient;
	std::shared_ptr<const OctData::Study  > actStudy  ;
	std::shared_ptr<const OctData::Series > actSeries ;

	mutable std::unique_ptr<SloBScanDistanceMap> seriesSLODistanceMap;
	
	std::unique_ptr<OctDataManagerThread> loadThread;
	
	OctDataManager();
	OctDataManager& operator=(const OctDataManager& other) = delete;
	
	boost::property_tree::ptree* getMarkerTreeSeries(const std::shared_ptr<const OctData::Series>& series);
	boost::property_tree::ptree* getMarkerTreeSeries(const std::shared_ptr<const OctData::Patient>& pat, const std::shared_ptr<const OctData::Study>& study, const std::shared_ptr<const OctData::Series>& series);
};

/**
 * @ingroup Manager
 * @brief
 *
 */
class OctDataManagerThread : public QThread, public CppFW::Callback
{
	Q_OBJECT

	OctDataManager& octDataManager;

	bool breakLoading = false;
	bool loadSuccess  = true;
	bool loadError    = false;

	std::unique_ptr<OctData::OCT> octData;

	const QString filename;
	QString  error;

public:
	OctDataManagerThread(OctDataManager& dataManager, const QString& filename);
	~OctDataManagerThread();

	void breakLoad()                                                { breakLoading = true; }

	bool success()                                           const  { return loadSuccess; }
	const QString& getError()                                const  { return error; }
	const QString& getFilename()                             const  { return filename; }
	bool hasLoadError()                                      const  { return loadError; }
	
	std::unique_ptr<OctData::OCT> getOctData();

protected:
	void run() override;

	bool callback(double frac) override
	{
		emit(stepCalulated(frac));
		return !breakLoading;
	}
signals:
	void stepCalulated(double);
};

