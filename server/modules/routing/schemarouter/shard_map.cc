/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2020-01-01
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

#include "shard_map.hh"

#include <algorithm>

#include <maxscale/alloc.h>

Shard::Shard():
    m_last_updated(time(NULL))
{
}

Shard::~Shard()
{
}

bool Shard::add_location(std::string db, SERVER* target)
{
    std::transform(db.begin(), db.end(), db.begin(), ::tolower);
    return m_map.insert(std::make_pair(db, target)).second;
}

void Shard::replace_location(std::string db, SERVER* target)
{
    std::transform(db.begin(), db.end(), db.begin(), ::tolower);
    m_map[db] = target;
}

SERVER* Shard::get_location(std::string db)
{
    SERVER* rval = NULL;
    std::transform(db.begin(), db.end(), db.begin(), ::tolower);
    ServerMap::iterator iter = m_map.find(db);

    if (iter != m_map.end())
    {
        rval = iter->second;
    }

    return rval;
}

bool Shard::stale(double max_interval) const
{
    time_t now = time(NULL);

    return difftime(now, m_last_updated) > max_interval;
}

bool Shard::empty() const
{
    return m_map.size() == 0;
}

void Shard::get_content(ServerMap& dest)
{
    for (ServerMap::iterator it = m_map.begin(); it != m_map.end(); it++)
    {
        dest.insert(*it);
    }
}

bool Shard::newer_than(const Shard& shard) const
{
    return m_last_updated > shard.m_last_updated;
}

ShardManager::ShardManager()
{
    spinlock_init(&m_lock);
}

ShardManager::~ShardManager()
{
}

Shard ShardManager::get_shard(std::string user, double max_interval)
{
    mxs::SpinLockGuard guard(m_lock);

    ShardMap::iterator iter = m_maps.find(user);

    if (iter == m_maps.end() || iter->second.stale(max_interval))
    {
        // No previous shard or a stale shard, construct a new one

        if (iter != m_maps.end())
        {
            m_maps.erase(iter);
        }

        return Shard();
    }

    // Found valid shard
    return iter->second;
}

void ShardManager::update_shard(Shard& shard, std::string user)
{
    mxs::SpinLockGuard guard(m_lock);
    ShardMap::iterator iter = m_maps.find(user);

    if (iter == m_maps.end() || shard.newer_than(iter->second))
    {
        m_maps[user] = shard;
    }
}
