/**
 * @file json_list_model.h
 * @brief 通用 JSON 列表模型：把 QJsonObject 列表转换成 QML ListView/TableView 可读取的动态角色。
 *
 * 调用链说明：QML 调用 Q_INVOKABLE/槽函数，Controller 通过 ApiClient 异步访问服务端，
 * 收到响应后更新 Q_PROPERTY 或列表模型并发出信号，QML 绑定会自动刷新。
 */
#pragma once

#include <QAbstractListModel>
#include <QJsonArray>
#include <QVariantMap>

namespace charging::admin {

class JsonListModel final : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role { RecordRole = Qt::UserRole + 1 };
    explicit JsonListModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}
    int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : rows_.size(); }
    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size() || role != RecordRole) return {};
        return rows_.at(index.row());
    }
    QHash<int,QByteArray> roleNames() const override { return {{RecordRole, "record"}}; }
    Q_INVOKABLE QVariantMap get(int row) const { return row >= 0 && row < rows_.size() ? rows_.at(row) : QVariantMap{}; }
    void setJson(const QJsonArray& values) {
        QList<QVariantMap> next;
        for (const auto& value : values) next.append(value.toObject().toVariantMap());
        if (next == rows_) return;
        beginResetModel(); rows_ = std::move(next); endResetModel();
    }
    void setRows(QList<QVariantMap> values) { beginResetModel(); rows_ = std::move(values); endResetModel(); }
private:
    QList<QVariantMap> rows_;
};

} // namespace charging::admin

