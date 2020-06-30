

#include <ripple/app/ledger/AcceptedLedger.h>
#include <ripple/app/ledger/InboundLedgers.h>
#include <ripple/app/ledger/LedgerMaster.h>
#include <ripple/app/main/Application.h>
#include <ripple/app/misc/NetworkOPs.h>
#include <ripple/basics/UptimeClock.h>
#include <ripple/core/DatabaseCon.h>
#include <ripple/json/json_value.h>
#include <ripple/ledger/CachedSLEs.h>
#include <ripple/net/RPCErr.h>
#include <ripple/nodestore/Database.h>
#include <ripple/nodestore/DatabaseShard.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/jss.h>
#include <ripple/rpc/Context.h>

namespace ripple {

static
void
textTime(std::string& text, UptimeClock::time_point& seconds,
         const char* unitName, std::chrono::seconds unitVal)
{
    auto i = seconds.time_since_epoch() / unitVal;

    if (i == 0)
        return;

    seconds -= unitVal * i;

    if (!text.empty ())
        text += ", ";

    text += std::to_string(i);
    text += " ";
    text += unitName;

    if (i > 1)
        text += "s";
}

Json::Value getCountsJson(Application& app, int minObjectCount)
{
    auto objectCounts = CountedObjects::getInstance().getCounts(minObjectCount);

    Json::Value ret(Json::objectValue);

    for (auto const& it : objectCounts)
    {
        ret [it.first] = it.second;
    }

    int dbKB = getKBUsedAll (app.getLedgerDB ().getSession ());

    if (dbKB > 0)
        ret[jss::dbKBTotal] = dbKB;

    dbKB = getKBUsedDB (app.getLedgerDB ().getSession ());

    if (dbKB > 0)
        ret[jss::dbKBLedger] = dbKB;

    dbKB = getKBUsedDB (app.getTxnDB ().getSession ());

    if (dbKB > 0)
        ret[jss::dbKBTransaction] = dbKB;

    {
        std::size_t c = app.getOPs().getLocalTxCount ();
        if (c > 0)
            ret[jss::local_txs] = static_cast<Json::UInt> (c);
    }

    ret[jss::write_load] = app.getNodeStore ().getWriteLoad ();

    ret[jss::historical_perminute] = static_cast<int>(
        app.getInboundLedgers().fetchRate());
    ret[jss::SLE_hit_rate] = app.cachedSLEs().rate();
    ret[jss::node_hit_rate] = app.getNodeStore ().getCacheHitRate ();
    ret[jss::ledger_hit_rate] = app.getLedgerMaster ().getCacheHitRate ();
    ret[jss::AL_hit_rate] = app.getAcceptedLedgerCache ().getHitRate ();

    ret[jss::fullbelow_size] = static_cast<int>(app.family().fullbelow().size());
    ret[jss::treenode_cache_size] = app.family().treecache().getCacheSize();
    ret[jss::treenode_track_size] = app.family().treecache().getTrackSize();

    std::string uptime;
    auto s = UptimeClock::now();
    using namespace std::chrono_literals;
    textTime (uptime, s, "year", 365 * 24h);
    textTime (uptime, s, "day", 24h);
    textTime (uptime, s, "hour", 1h);
    textTime (uptime, s, "minute", 1min);
    textTime (uptime, s, "second", 1s);
    ret[jss::uptime] = uptime;

    ret[jss::node_writes] = app.getNodeStore().getStoreCount();
    ret[jss::node_reads_total] = app.getNodeStore().getFetchTotalCount();
    ret[jss::node_reads_hit] = app.getNodeStore().getFetchHitCount();
    ret[jss::node_written_bytes] = app.getNodeStore().getStoreSize();
    ret[jss::node_read_bytes] = app.getNodeStore().getFetchSize();

    if (auto shardStore = app.getShardStore())
    {
        Json::Value& jv = (ret[jss::shards] = Json::objectValue);
        jv[jss::fullbelow_size] =
            static_cast<int>(app.shardFamily()->fullbelow().size());
        jv[jss::treenode_cache_size] =
            app.shardFamily()->treecache().getCacheSize();
        jv[jss::treenode_track_size] =
            app.shardFamily()->treecache().getTrackSize();
        ret[jss::write_load] = shardStore->getWriteLoad();
        ret[jss::node_hit_rate] = shardStore->getCacheHitRate();
        jv[jss::node_writes] = shardStore->getStoreCount();
        jv[jss::node_reads_total] = shardStore->getFetchTotalCount();
        jv[jss::node_reads_hit] = shardStore->getFetchHitCount();
        jv[jss::node_written_bytes] = shardStore->getStoreSize();
        jv[jss::node_read_bytes] = shardStore->getFetchSize();
    }

    return ret;
}

Json::Value doGetCounts (RPC::Context& context)
{
    int minCount = 10;

    if (context.params.isMember (jss::min_count))
        minCount = context.params[jss::min_count].asUInt ();

    return getCountsJson(context.app, minCount);
}

} 





