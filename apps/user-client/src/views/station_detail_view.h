#pragma once
#include <QJsonObject>
#include <QWidget>
class QLabel; class QVBoxLayout;
namespace charging::user {
class StationDetailView final : public QWidget {
    Q_OBJECT
public:
    explicit StationDetailView(QWidget* parent=nullptr);
    void setStation(const QJsonObject& station);
    void setPiles(const QJsonArray& piles, bool canReserve);
signals:
    void backRequested();
    void navigationRequested(const QString& mode, const QJsonObject& station);
    void reservationRequested(qint64 pileId, double powerKw);
private:
    QJsonObject station_;
    QLabel* summary_=nullptr;
    QVBoxLayout* piles_=nullptr;
};
}
