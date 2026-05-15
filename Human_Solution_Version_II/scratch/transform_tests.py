import os
import random
import glob

def parse_time_block(s):
    day = s[:3]
    hour = int(s[3:])
    days = {"Mon": 1, "Tue": 2, "Wed": 3, "Thu": 4, "Fri": 5, "Sat": 6, "Sun": 7}
    return days[day] * 100 + hour

def format_time_block(t):
    days = {1: "Mon", 2: "Tue", 3: "Wed", 4: "Thu", 5: "Fri", 6: "Sat", 7: "Sun"}
    d = t // 100
    h = t % 100
    return f"{days[d]}{h:02d}" if h < 10 else f"{days[d]}{h}"

def generate_events(active_slots, active_requests):
    C = []
    U = []
    Q = []
    
    # Simple probabilities
    prob_c = 0.05
    prob_u = 0.05
    prob_q = 0.05
    
    for s_id, s_time in list(active_slots.items()):
        r = random.random()
        if r < prob_c:
            C.append(f"{s_id}:{s_time}")
            del active_slots[s_id]
        elif r < prob_c + prob_u:
            # Reschedule
            t_val = parse_time_block(s_time)
            new_t = min(723, t_val + random.randint(1, 23))
            new_time_str = format_time_block(new_t)
            U.append(f"{s_id} {s_time} {new_time_str}")
            active_slots[s_id] = new_time_str
            
    for r_id, times in list(active_requests.items()):
        if random.random() < prob_q:
            # pick a random time from its acceptable times to cancel it
            if times:
                q_time = random.choice(times)
                Q.append(f"{r_id}:{q_time}")
            else:
                Q.append(f"{r_id}:Mon00")
            del active_requests[r_id]
            
    return C, U, Q

def process_file(filepath):
    with open(filepath, 'r') as f:
        lines = [l.strip() for l in f.readlines() if l.strip()]
        
    out_lines = []
    idx = 0
    # Read Fellows
    out_lines.append(lines[idx])
    idx += 1
    # Read Mentors
    out_lines.append(lines[idx])
    num_mentors = len(lines[idx].split())
    idx += 1
    # Read Mentor topics
    for _ in range(num_mentors):
        out_lines.append(lines[idx])
        idx += 1
        
    global_slot_id = 1
    global_req_id = 1
    
    active_slots = {}
    active_requests = {}
    
    while idx < len(lines):
        line = lines[idx]
        if line.startswith("WEEK"):
            out_lines.append(line)
            idx += 1
            
            # Read Slots
            num_slots = int(lines[idx])
            out_lines.append(str(num_slots))
            idx += 1
            
            if num_slots > 0:
                slots = lines[idx].split()
                for s in slots:
                    mentor, t_block = s.split(':')
                    out_lines.append(f"{global_slot_id} {mentor} {t_block}")
                    active_slots[global_slot_id] = t_block
                    global_slot_id += 1
                idx += 1
                
            # Read Requests
            num_reqs = int(lines[idx])
            out_lines.append(str(num_reqs))
            idx += 1
            
            for _ in range(num_reqs):
                req_line = lines[idx]
                parts = req_line.split()
                out_lines.append(f"{global_req_id} {req_line}")
                
                times = parts[2].split(',') if parts[2] != '""' else []
                active_requests[global_req_id] = times
                
                global_req_id += 1
                idx += 1
                
            # Generate events for this week
            C, U, Q = generate_events(active_slots, active_requests)
            
            out_lines.append(str(len(C)))
            if C:
                out_lines.append(" ".join(C))
            else:
                out_lines.append("")
                
            out_lines.append(str(len(U)))
            for u in U:
                out_lines.append(u)
                
            out_lines.append(str(len(Q)))
            if Q:
                out_lines.append(" ".join(Q))
            else:
                out_lines.append("")
        else:
            idx += 1

    with open(filepath, 'w') as f:
        f.write("\n".join(out_lines) + "\n")

if __name__ == "__main__":
    test_files = glob.glob("tests/*.txt")
    for tf in test_files:
        process_file(tf)
    print("Tests transformed successfully.")
