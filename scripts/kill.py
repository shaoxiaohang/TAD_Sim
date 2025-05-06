#!/usr/bin/env python3
import psutil
import sys

def kill_processes(target_names):
    killed = []
    zombies = []

    for proc in psutil.process_iter(['pid', 'name', 'status', 'ppid']):
        try:
            # 获取进程详细信息
            p_name = proc.info['name']
            p_pid = proc.pid
            p_status = proc.status()

            if p_name not in target_names:
                continue

            # 处理僵尸进程
            if p_status == psutil.STATUS_ZOMBIE:
                zombies.append({
                    'name': p_name,
                    'pid': p_pid,
                    'ppid': proc.info['ppid']
                })
                continue

            # 终止流程
            status = "terminated"
            proc.kill()
            status = "force killed"
            
            killed.append({'proc': proc, 'status': status})

        except (psutil.NoSuchProcess, psutil.AccessDenied) as e:
            print(f"⚠️  操作失败: {p_name} (PID:{p_pid}) - {str(e)}", file=sys.stderr)

    return killed, zombies

def clean_zombies(zombie_list):
    cleaned = []
    for z in zombie_list:
        try:
            # 尝试终止父进程
            parent = psutil.Process(z['ppid'])
            parent.kill()
            cleaned.append(z)
        except Exception as e:
            print(f"⚠️  无法清理 {z['name']}({z['pid']}) 的父进程 {z['ppid']}: {str(e)}", 
                 file=sys.stderr)
    return cleaned

if __name__ == "__main__":
    target_processes = ["Display"]
    
    
    # 终止活动进程
    killed_procs, zombies = kill_processes(target_processes)
    
    # 打印活动进程终止结果
    print("\n🔍 终止结果:")
    if killed_procs:
        for item in killed_procs:
            p = item['proc']
            print(f"❌ {item['status']}: {p.info['name']} (PID:{p.pid})")
    else:
        print("⭕ 无活动进程被终止")

    # 处理僵尸进程
    if zombies:
        print("\n🧟 发现僵尸进程:")
        for z in zombies:
            print(f"☠️  {z['name']}(PID:{z['pid']}) ← 父进程 PPID:{z['ppid']}")

        # 交互式清理
        choice = input("\n是否尝试终止父进程来清理僵尸？(y/N): ").strip().lower()
        if choice == 'y':
            cleaned = clean_zombies(zombies)
            if cleaned:
                print("\n🗑️ 已清理的僵尸进程:")
                for z in cleaned:
                    print(f"✅ 通过终止父进程 PPID:{z['ppid']} 清理: {z['name']}(PID:{z['pid']})")
            else:
                print("\n⚠️  未能清理任何僵尸进程")

    # 最终状态检查
    remaining = []
    for p in psutil.process_iter(['name', 'pid', 'status']):
        try:
            if p.info['name'] in target_processes and p.status() != psutil.STATUS_ZOMBIE:
                remaining.append(f"{p.info['name']}(PID:{p.pid})")
        except psutil.NoSuchProcess:
            continue
    
    print("\n🔎 最终状态:")
    if remaining:
        print(f"🚨 残留活动进程: {', '.join(remaining)}", file=sys.stderr)
        sys.exit(1)
    else:
        print("✅ 所有目标活动进程已清除")
        sys.exit(0)