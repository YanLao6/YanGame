# YanGame Perforce 提交记录与作者统计

在本次查询可见的项目路径内，全部 304 条已提交 changelist 均归属于 Perforce 账户 yanlao，未发现其他账户的提交记录。

- 导出时间：2026-10-06 01:24:26 +08:00
- 查询范围：//Simple/Dev/Projecties/YanGame/...
- 提交状态：submitted；未按用户过滤；未限制记录数量
- 记录数量：304 条 changelist（不是文件数，也不是工作时长）
- 最早提交：2026-04-18 13:39:36 +08:00（CL 14）
- 最近提交：2026-10-06 01:20:12 +08:00（CL 319）
- 时间统一显示为 UTC+08:00
- 交叉验证：使用项目本地路径再次查询，提交编号集合完全一致；同时检查状态与编号唯一性。

| Perforce 账户 | 提交数 | 占比 |
| --- | ---: | ---: |
| yanlao | 304 | 100.00% |

## 可用于项目说明的表述

“YanGame 由我独立开发和维护。本附件提供项目路径内的 Perforce 提交记录作为开发过程的佐证：在本次查询可见的项目路径内，全部 304 条已提交 changelist 均归属于 Perforce 账户 yanlao，未发现其他账户的提交记录。”

其中“由我独立开发和维护”是本人声明；Perforce 数据直接支持的是提交账户归属。账户与自然人的对应关系、账户是否共享，以及导入代码、第三方插件和素材的原创归属，不由该统计独立证明。项目路径外的其他分支、不可见或已清除的历史，以及未提交的开发活动，均不在本次统计范围内。

## 文件与复核

- report.html：作者统计和完整提交明细，浏览器打开即可查看或打印。
- changes.txt：按提交时间升序排列，保留完整原始提交说明。
- changes.raw.jsonl：Perforce 返回的原始结构化记录（含 Unix 时间戳）。
- SHA256SUMS.txt：导出文件校验值，用于检查导出后内容是否改变；不是服务器签名或独立公证。

复核命令（PowerShell，在具备对应服务器访问权限的环境中执行）：

& "C:\Program Files\Perforce\p4.exe" -p localhost:1666 -u yanlao -c UnreaEngine -ztag -Mj changes -s submitted -l "//Simple/Dev/Projecties/YanGame/..."

本机连接参数来自项目 .p4config。报告只覆盖当前账户可读取的记录。完整提交说明按服务器内容保留，未删改不利或不完整的说明。