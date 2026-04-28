#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <sstream>
#include <exception>
#include <ctime>

using namespace std;

// ============================================================
//  UTILITY NAMESPACE
// ============================================================
namespace Utils {
    template <typename T>
    void printVector(const vector<T>& vec, const string& emptyMsg) {
        if (vec.empty()) { cout << "   " << emptyMsg << "\n"; return; }
        for (const auto& item : vec) cout << "   > " << item << "\n";
    }

    string escape(string s)   { replace(s.begin(), s.end(), ' ', '_'); return s; }
    string unescape(string s) { replace(s.begin(), s.end(), '_', ' '); return s; }

    bool containsCI(const string& hay, const string& ndl) {
        string h=hay, n=ndl;
        transform(h.begin(),h.end(),h.begin(),::tolower);
        transform(n.begin(),n.end(),n.begin(),::tolower);
        return h.find(n)!=string::npos;
    }

    void printProgressBar(int value, int max, int width=28) {
        int f=(max>0)?(value*width/max):0;
        cout<<"["; for(int i=0;i<width;i++) cout<<(i<f?"#":".");  cout<<"] "<<value<<"/"<<max;
    }

    string now() {
        auto t=chrono::system_clock::to_time_t(chrono::system_clock::now());
        string s(ctime(&t));
        if(!s.empty()&&s.back()=='\n') s.pop_back();
        return s;
    }

    void writeSection(ofstream& f, const string& title) {
        f << "\n"
          << "################################################################################\n"
          << "#  " << left << setw(76) << title << " #\n"
          << "################################################################################\n";
    }
    void writeSubSection(ofstream& f, const string& title) {
        f << "\n"
          << "  +----------------------------------------------------------+\n"
          << "  |  " << left << setw(56) << title << "  |\n"
          << "  +----------------------------------------------------------+\n";
    }
    void writeDivider(ofstream& f, char ch='-', int w=70, int indent=4) {
        f << string(indent,' ') << string(w,ch) << "\n";
    }
}

// ============================================================
//  EXCEPTION
// ============================================================
class SecurityException : public exception {
    string msg;
public:
    explicit SecurityException(const string& m) : msg(m) {}
    const char* what() const noexcept override { return msg.c_str(); }
};

// ============================================================
//  DATA STRUCTURES
// ============================================================
namespace InstituteCore {

    struct PlacementRecord {
        string company, role, package, offerDate, joiningDate, status;
    };

    struct User {
        string id, password, name;
        vector<string> messages;
        virtual void printRole() const { cout << "[User: " << name << "]\n"; }
        virtual ~User() = default;
    };

    struct Trainee : public User {
        vector<string>          enrolledPrograms;
        vector<string>          pendingPrograms;
        map<string,string>      projectGrades;
        map<string,int>         attendedClasses;
        float                   feeBalance=0, totalPaid=0;
        bool                    placementEligible=false;
        string                  placementStatus;
        string                  preferredCompany, preferredRole, resume;
        int                     placementScore=0;
        vector<PlacementRecord> placementHistory;
        vector<string>          paymentHistory;

        void printRole() const override {
            string ps = placementStatus.empty() ? "Not Applied" : placementStatus;
            cout << "\n[TRAINEE | " << name << " | Placement: " << ps
                 << " | Dues: $" << fixed << setprecision(2) << feeBalance << "]\n";
        }
        Trainee& operator+=(float fee) { feeBalance+=fee; return *this; }
        Trainee& operator-=(float payment) {
            float a=min(payment,feeBalance); feeBalance-=a; totalPaid+=a;
            paymentHistory.push_back("$"+to_string((int)a)+"|"+Utils::now());
            return *this;
        }
    };

    struct Trainer : public User {
        string domain; vector<string> activeBatches; float salary=0;
        void printRole() const override {
            cout << "\n[TRAINER | " << name << " | " << domain
                 << " | Salary: $" << fixed << setprecision(2) << salary << "]\n";
        }
    };

    struct Manager : public User {
        string domain;
        void printRole() const override { cout << "\n[MANAGER | " << name << "]\n"; }
    };

    struct TrainingProgram {
        string id, name, domain, scheduleTime;
        int    durationWeeks=0, maxSeats=0, enrolledSeats=0;
        float  feeAmount=0;
        string assignedTrainerId;
        int    totalTopics=0, completedTopics=0, totalClassesHeld=0;
        vector<string> modules;
        string certExamQuestion, certExamAnswer;
        int    examDurationMins=0;
        int    placedCount=0; float avgPackage=0; string topRecruiter;

        static void printCourseHeader() {
            cout << left
                 << setw(8)<<"ID" << setw(38)<<"COURSE NAME" << setw(14)<<"DOMAIN"
                 << setw(20)<<"SCHEDULE" << setw(5)<<"WKS" << setw(9)<<"FEE" << "SEATS\n"
                 << string(104,'-') << "\n";
        }
        friend ostream& operator<<(ostream& os, const TrainingProgram& p) {
            string sn=p.name.length()>36?p.name.substr(0,33)+"...":p.name;
            string ss=p.scheduleTime.length()>18?p.scheduleTime.substr(0,15)+"...":p.scheduleTime;
            os << left << setw(8)<<p.id << setw(38)<<sn << setw(14)<<p.domain
               << setw(20)<<ss << setw(5)<<p.durationWeeks
               << "$" << setw(8)<<p.feeAmount << p.enrolledSeats<<"/"<<p.maxSeats;
            return os;
        }
    };

    struct Inquiry     { string id, name, contact, question, status; };
    struct Appointment { string id, name, purpose, date, status; };
}
using namespace InstituteCore;

// ============================================================
//  MAIN SYSTEM
// ============================================================
class TrainingInstitute {
private:
    unordered_map<string,Trainee>         trainees;
    unordered_map<string,Trainer>         trainers;
    unordered_map<string,Manager>         managers;
    unordered_map<string,TrainingProgram> programs;
    unordered_map<string,Inquiry>         inquiries;
    unordered_map<string,Appointment>     appointments;
    vector<string> placementPool, hiringPartners;
    string currentUser, currentRole;

    // ========== FILE INTEGRATION ==========

    void saveData() {
        ofstream db("institute_databasemanagement.txt");
        if (!db.is_open()) { cerr << "[ERROR] Cannot open database.\n"; return; }

        db << "################################################################################\n"
           << "#        MODERN TECH TRAINING INSTITUTE - DATABASE FILE                       #\n"
           << "#   Generated : " << left << setw(62) << Utils::now() << "#\n"
           << "################################################################################\n";

        // SECTION 1: USERS
        Utils::writeSection(db, "SECTION 1 : USERS");
        Utils::writeSubSection(db, "1.1  TRAINEES");
        db << "  " << left << setw(14)<<"# RECORD" << setw(12)<<"ID" << setw(18)<<"PASSWORD"
           << setw(25)<<"NAME" << setw(10)<<"BALANCE" << setw(10)<<"PAID"
           << setw(6) <<"ELIG" << setw(28)<<"PLACEMENT_STATUS" << "PREF_COMPANY\n"
           << "  " << string(130,'-') << "\n";
        for (const auto& it : trainees) {
            const Trainee& tr = it.second;
            db << "  TRAINEE      " << left << setw(12)<<tr.id << setw(18)<<tr.password
               << setw(25)<<Utils::escape(tr.name) << setw(10)<<fixed<<setprecision(2)<<tr.feeBalance
               << setw(10)<<tr.totalPaid << setw(6) <<tr.placementEligible
               << setw(28)<<(tr.placementStatus.empty()?"None":Utils::escape(tr.placementStatus))
               << (tr.preferredCompany.empty()?"None":Utils::escape(tr.preferredCompany)) << "\n";
        }

        Utils::writeSubSection(db, "1.2  TRAINERS");
        db << "  " << left << setw(14)<<"# RECORD" << setw(12)<<"ID" << setw(18)<<"PASSWORD"
           << setw(25)<<"NAME" << setw(20)<<"DOMAIN" << "SALARY\n"
           << "  " << string(95,'-') << "\n";
        for (const auto& it : trainers) {
            const Trainer& tr = it.second;
            db << "  TRAINER      " << left << setw(12)<<tr.id << setw(18)<<tr.password
               << setw(25)<<Utils::escape(tr.name) << setw(20)<<Utils::escape(tr.domain)
               << fixed<<setprecision(2)<<tr.salary << "\n";
        }

        Utils::writeSubSection(db, "1.3  MANAGERS");
        db << "  " << left << setw(14)<<"# RECORD" << setw(12)<<"ID" << setw(18)<<"PASSWORD"
           << setw(25)<<"NAME" << "DEPARTMENT\n"
           << "  " << string(80,'-') << "\n";
        for (const auto& it : managers) {
            const Manager& m = it.second;
            db << "  MANAGER      " << left << setw(12)<<m.id << setw(18)<<m.password
               << setw(25)<<Utils::escape(m.name) << Utils::escape(m.domain) << "\n";
        }

        // SECTION 2: PROGRAMS
        Utils::writeSection(db, "SECTION 2 : TRAINING PROGRAMS");
        db << "  " << left << setw(14)<<"# RECORD" << setw(10)<<"ID" << setw(33)<<"NAME"
           << setw(14)<<"DOMAIN" << setw(20)<<"SCHEDULE" << setw(5)<<"WKS"
           << setw(6) <<"MAX" << setw(6) <<"ENR" << setw(10)<<"FEE"
           << setw(12)<<"TRAINER" << setw(6) <<"TTOP" << setw(6) <<"CTOP"
           << setw(6) <<"CLS" << setw(6) <<"PLCD" << setw(8) <<"AVGPKG" << "TOP_RECRUITER\n"
           << "  " << string(162,'-') << "\n";
        for (const auto& it : programs) {
            const TrainingProgram& pg = it.second;
            db << "  PROGRAM      " << left << setw(10)<<pg.id << setw(33)<<Utils::escape(pg.name)
               << setw(14)<<Utils::escape(pg.domain) << setw(20)<<Utils::escape(pg.scheduleTime)
               << setw(5)<<pg.durationWeeks << setw(6)<<pg.maxSeats << setw(6)<<pg.enrolledSeats
               << setw(10)<<fixed<<setprecision(2)<<pg.feeAmount
               << setw(12)<<(pg.assignedTrainerId.empty()?"None":pg.assignedTrainerId)
               << setw(6)<<pg.totalTopics << setw(6)<<pg.completedTopics << setw(6)<<pg.totalClassesHeld
               << setw(6)<<pg.placedCount << setw(8)<<pg.avgPackage
               << (pg.topRecruiter.empty()?"None":Utils::escape(pg.topRecruiter)) << "\n"
               << "               EXAM_Q=" << (pg.certExamQuestion.empty()?"None":Utils::escape(pg.certExamQuestion))
               << "  EXAM_A=" << (pg.certExamAnswer.empty()?"None":Utils::escape(pg.certExamAnswer))
               << "  EXAM_MINS=" << pg.examDurationMins << "\n";
        }

        // SECTION 3: FRONT DESK
        Utils::writeSection(db, "SECTION 3 : FRONT DESK");
        Utils::writeSubSection(db, "3.1  INQUIRIES");
        db << "  " << left << setw(14)<<"# RECORD" << setw(12)<<"ID" << setw(22)<<"NAME"
           << setw(16)<<"CONTACT" << setw(12)<<"STATUS" << "QUESTION\n"
           << "  " << string(105,'-') << "\n";
        for (const auto& it : inquiries) {
            const Inquiry& i = it.second;
            db << "  INQUIRY      " << left << setw(12)<<i.id << setw(22)<<Utils::escape(i.name)
               << setw(16)<<i.contact << setw(12)<<Utils::escape(i.status)
               << Utils::escape(i.question) << "\n";
        }

        Utils::writeSubSection(db, "3.2  APPOINTMENTS");
        db << "  " << left << setw(14)<<"# RECORD" << setw(12)<<"ID" << setw(22)<<"NAME"
           << setw(25)<<"PURPOSE" << setw(22)<<"DATE" << "STATUS\n"
           << "  " << string(100,'-') << "\n";
        for (const auto& it : appointments) {
            const Appointment& a = it.second;
            db << "  APPOINT      " << left << setw(12)<<a.id << setw(22)<<Utils::escape(a.name)
               << setw(25)<<Utils::escape(a.purpose) << setw(22)<<Utils::escape(a.date)
               << Utils::escape(a.status) << "\n";
        }

        // SECTION 4: TRAINEE RECORDS
        Utils::writeSection(db, "SECTION 4 : TRAINEE RECORDS");
        for (const auto& it : trainees) {
            const Trainee& tr = it.second;
            Utils::writeSubSection(db, "Trainee: " + tr.id + "  (" + tr.name + ")");
            db << "    [Enrolled Programs]\n";
            if (tr.enrolledPrograms.empty()) db << "    (none)\n";
            for (const string& pid : tr.enrolledPrograms)
                db << "    ENROLL        " << tr.id << "  " << pid << "\n";
            db << "    [Pending Applications]\n";
            if (tr.pendingPrograms.empty()) db << "    (none)\n";
            for (const string& pid : tr.pendingPrograms)
                db << "    PENDING       " << tr.id << "  " << pid << "\n";
            db << "    [Project Grades]\n";
            if (tr.projectGrades.empty()) db << "    (none)\n";
            for (const auto& gradeIt : tr.projectGrades)
                db << "    GRADE         " << tr.id << "  " << left<<setw(10)<<gradeIt.first << "  " << gradeIt.second << "\n";
            db << "    [Attendance]\n";
            if (tr.attendedClasses.empty()) db << "    (none)\n";
            for (const auto& attIt : tr.attendedClasses)
                db << "    ATTEND        " << tr.id << "  " << left<<setw(10)<<attIt.first << "  " << attIt.second << "\n";
            db << "    [Payment History]\n";
            if (tr.paymentHistory.empty()) db << "    (none)\n";
            for (const string& rec : tr.paymentHistory) {
                string safe=rec; replace(safe.begin(),safe.end(),' ','_');
                db << "    PAYMENT       " << tr.id << "  " << safe << "\n";
            }
            Utils::writeDivider(db,'.',72,4);
        }

        // SECTION 5: PLACEMENT
        Utils::writeSection(db, "SECTION 5 : PLACEMENT DATA");
        Utils::writeSubSection(db, "5.1  PLACEMENT PROFILES");
        db << "  " << left << setw(14)<<"# RECORD" << setw(12)<<"TRAINEE_ID"
           << setw(22)<<"PREF_ROLE" << setw(30)<<"RESUME" << setw(6)<<"SCORE\n"
           << "  " << string(90,'-') << "\n";
        for (const auto& it : trainees) {
            const Trainee& tr = it.second;
            db << "  PLAC_PROFILE  " << left << setw(12)<<tr.id
               << setw(22)<<(tr.preferredRole.empty()?"None":Utils::escape(tr.preferredRole))
               << setw(30)<<(tr.resume.empty()?"None":Utils::escape(tr.resume))
               << tr.placementScore << "\n";
        }

        Utils::writeSubSection(db, "5.2  PLACEMENT HISTORY");
        db << "  " << left << setw(14)<<"# RECORD" << setw(12)<<"TRAINEE_ID"
           << setw(20)<<"COMPANY" << setw(20)<<"ROLE" << setw(12)<<"PACKAGE"
           << setw(14)<<"OFFER_DATE" << setw(14)<<"JOIN_DATE" << "STATUS\n"
           << "  " << string(120,'-') << "\n";
        for (const auto& it : trainees) {
            const Trainee& tr = it.second;
            for (const auto& pr : tr.placementHistory) {
                db << "  PLAC_REC      " << left << setw(12)<<tr.id
                   << setw(20)<<Utils::escape(pr.company) << setw(20)<<Utils::escape(pr.role)
                   << setw(12)<<Utils::escape(pr.package) << setw(14)<<Utils::escape(pr.offerDate)
                   << setw(14)<<Utils::escape(pr.joiningDate) << Utils::escape(pr.status) << "\n";
            }
        }

        Utils::writeSubSection(db, "5.3  ACTIVE PLACEMENT POOL");
        if (placementPool.empty()) db << "    (empty)\n";
        for (const string& pid : placementPool) db << "    POOL          " << pid << "\n";

        Utils::writeSubSection(db, "5.4  HIRING PARTNERS");
        for (const string& hp : hiringPartners) db << "    PARTNER       " << Utils::escape(hp) << "\n";

        // SECTION 6: PROGRAM MODULES
        Utils::writeSection(db, "SECTION 6 : PROGRAM MODULES");
        for (const auto& it : programs) {
            const TrainingProgram& pg = it.second;
            if (pg.modules.empty()) continue;
            db << "\n  [" << pg.id << "  -  " << pg.name << "]\n";
            for (const string& mod : pg.modules)
                db << "    MODULE        " << it.first << "  " << Utils::escape(mod) << "\n";
        }

        // SECTION 7: TRAINER BATCHES
        Utils::writeSection(db, "SECTION 7 : TRAINER BATCH ASSIGNMENTS");
        for (const auto& it : trainers) {
            const Trainer& tr = it.second;
            if (tr.activeBatches.empty()) continue;
            db << "\n  [" << tr.id << "  -  " << tr.name << "]\n";
            for (const string& bid : tr.activeBatches)
                db << "    BATCH         " << tr.id << "  " << bid << "\n";
        }

        // SECTION 8: MESSAGE QUEUES
        Utils::writeSection(db, "SECTION 8 : PENDING MESSAGE QUEUES");
        for (const auto& it : trainees)
            for (const string& msg : it.second.messages)
                db << "  MSG_TRAINEE   " << it.first << "  " << Utils::escape(msg) << "\n";
        for (const auto& it : trainers)
            for (const string& msg : it.second.messages)
                db << "  MSG_TRAINER   " << it.first << "  " << Utils::escape(msg) << "\n";
        for (const auto& it : managers)
            for (const string& msg : it.second.messages)
                db << "  MSG_MANAGER   " << it.first << "  " << Utils::escape(msg) << "\n";

        db << "\n################################################################################\n"
           << "#  END OF FILE                                                                 #\n"
           << "################################################################################\n";
    }

    void loadData() {
        ifstream db("institute_databasemanagement.txt");
        if (!db.is_open()) return;

        string token;
        while (db >> token) {
            if (token.empty() || token[0]=='#' || token[0]=='+' || token[0]=='|' ||
                token[0]=='-' || token[0]=='.' || token=="===" || token=="SECTION" ||
                token=="END" || token=="OF" || token=="FILE" || token=="(none)" || token=="(empty)") {
                db.ignore(1000,'\n'); continue;
            }
            string id, e1, e2;

            if (token=="TRAINEE") {
                string pw,n,ps,pref; float fee,paid; bool elig;
                db>>id>>pw>>n>>fee>>paid>>elig>>ps>>pref;
                Trainee tr; tr.id=id; tr.password=pw; tr.name=Utils::unescape(n);
                tr.feeBalance=fee; tr.totalPaid=paid; tr.placementEligible=elig;
                tr.placementStatus=(ps=="None"?"":Utils::unescape(ps));
                tr.preferredCompany=(pref=="None"?"":Utils::unescape(pref));
                trainees[id]=tr; db.ignore(1000,'\n');
            }
            else if (token=="TRAINER") {
                string pw,n,dom; float sal;
                db>>id>>pw>>n>>dom>>sal;
                Trainer tr; tr.id=id; tr.password=pw; tr.name=Utils::unescape(n);
                tr.domain=Utils::unescape(dom); tr.salary=sal;
                trainers[id]=tr; db.ignore(1000,'\n');
            }
            else if (token=="MANAGER") {
                string pw,n,dom;
                db>>id>>pw>>n>>dom;
                Manager m; m.id=id; m.password=pw; m.name=Utils::unescape(n);
                m.domain=Utils::unescape(dom);
                managers[id]=m; db.ignore(1000,'\n');
            }
            else if (token=="PROGRAM") {
                string n,dom,sched,trId,topRec;
                int dur,ms,es,tTop,cTop,tClass,plcd; float fee,avgPkg;
                db>>id>>n>>dom>>sched>>dur>>ms>>es>>fee>>trId>>tTop>>cTop>>tClass>>plcd>>avgPkg>>topRec;
                TrainingProgram p; p.id=id; p.name=Utils::unescape(n); p.domain=Utils::unescape(dom);
                p.scheduleTime=Utils::unescape(sched); p.durationWeeks=dur; p.maxSeats=ms; p.enrolledSeats=es;
                p.feeAmount=fee; p.assignedTrainerId=(trId=="None"?"":trId);
                p.totalTopics=tTop; p.completedTopics=cTop; p.totalClassesHeld=tClass;
                p.placedCount=plcd; p.avgPackage=avgPkg;
                p.topRecruiter=(topRec=="None"?"":Utils::unescape(topRec));
                programs[id]=p;
                string examLine; getline(db,examLine);
                if (!examLine.empty()) {
                    size_t qpos=examLine.find("EXAM_Q=");
                    if(qpos!=string::npos){
                        qpos+=7; size_t qend=examLine.find("  EXAM_A=",qpos);
                        if(qend!=string::npos){
                            string q=examLine.substr(qpos,qend-qpos);
                            programs[id].certExamQuestion=(q=="None"?"":Utils::unescape(q));
                        }
                    }
                    size_t apos=examLine.find("EXAM_A=");
                    if(apos!=string::npos){
                        apos+=7; size_t aend=examLine.find("  EXAM_MINS=",apos);
                        if(aend!=string::npos){
                            string a=examLine.substr(apos,aend-apos);
                            programs[id].certExamAnswer=(a=="None"?"":Utils::unescape(a));
                        }
                    }
                    size_t mpos=examLine.find("EXAM_MINS=");
                    if(mpos!=string::npos){
                        mpos+=10; string m=examLine.substr(mpos);
                        programs[id].examDurationMins=atoi(m.c_str());
                    }
                }
            }
            else if (token=="ENROLL")   { db>>id>>e1; if(trainees.count(id)) trainees[id].enrolledPrograms.push_back(e1); db.ignore(1000,'\n'); }
            else if (token=="PENDING")  { db>>id>>e1; if(trainees.count(id)) trainees[id].pendingPrograms.push_back(e1); db.ignore(1000,'\n'); }
            else if (token=="GRADE")    { db>>id>>e1>>e2; if(trainees.count(id)) trainees[id].projectGrades[e1]=e2; db.ignore(1000,'\n'); }
            else if (token=="ATTEND")   { db>>id>>e1>>e2; if(trainees.count(id)) trainees[id].attendedClasses[e1]=atoi(e2.c_str()); db.ignore(1000,'\n'); }
            else if (token=="PAYMENT")  { db>>id>>e1; string r=e1; replace(r.begin(),r.end(),'_',' '); if(trainees.count(id)) trainees[id].paymentHistory.push_back(r); db.ignore(1000,'\n'); }
            else if (token=="PLAC_PROFILE") {
                string role,res; int score;
                db>>id>>role>>res>>score;
                if(trainees.count(id)){
                    trainees[id].preferredRole=(role=="None"?"":Utils::unescape(role));
                    trainees[id].resume=(res=="None"?"":Utils::unescape(res));
                    trainees[id].placementScore=score;
                }
                db.ignore(1000,'\n');
            }
            else if (token=="PLAC_REC") {
                string co,role,pkg,od,jd,st;
                db>>id>>co>>role>>pkg>>od>>jd>>st;
                if(trainees.count(id)){
                    PlacementRecord pr;
                    pr.company=Utils::unescape(co); pr.role=Utils::unescape(role);
                    pr.package=Utils::unescape(pkg); pr.offerDate=Utils::unescape(od);
                    pr.joiningDate=Utils::unescape(jd); pr.status=Utils::unescape(st);
                    trainees[id].placementHistory.push_back(pr);
                }
                db.ignore(1000,'\n');
            }
            else if (token=="MODULE") { db>>id>>e1; if(programs.count(id)) programs[id].modules.push_back(Utils::unescape(e1)); db.ignore(1000,'\n'); }
            else if (token=="BATCH")  { db>>id>>e1; if(trainers.count(id)){ vector<string>& b=trainers[id].activeBatches; if(find(b.begin(),b.end(),e1)==b.end()) b.push_back(e1); } db.ignore(1000,'\n'); }
            else if (token=="INQUIRY") {
                string n,c,s,q; db>>id>>n>>c>>s>>q;
                Inquiry i; i.id=id; i.name=Utils::unescape(n); i.contact=c;
                i.status=Utils::unescape(s); i.question=Utils::unescape(q);
                inquiries[id]=i; db.ignore(1000,'\n');
            }
            else if (token=="APPOINT") {
                string n,pu,d,s; db>>id>>n>>pu>>d>>s;
                Appointment a; a.id=id; a.name=Utils::unescape(n); a.purpose=Utils::unescape(pu);
                a.date=Utils::unescape(d); a.status=Utils::unescape(s);
                appointments[id]=a; db.ignore(1000,'\n');
            }
            else if (token=="POOL")        { db>>e1; placementPool.push_back(e1); db.ignore(1000,'\n'); }
            else if (token=="PARTNER")     { db>>e1; hiringPartners.push_back(Utils::unescape(e1)); db.ignore(1000,'\n'); }
            else if (token=="MSG_TRAINEE") { db>>id>>e1; if(trainees.count(id)) trainees[id].messages.push_back(Utils::unescape(e1)); db.ignore(1000,'\n'); }
            else if (token=="MSG_TRAINER") { db>>id>>e1; if(trainers.count(id)) trainers[id].messages.push_back(Utils::unescape(e1)); db.ignore(1000,'\n'); }
            else if (token=="MSG_MANAGER") { db>>id>>e1; if(managers.count(id)) managers[id].messages.push_back(Utils::unescape(e1)); db.ignore(1000,'\n'); }
            else { db.ignore(1000,'\n'); }
        }
    }

    void addTrainee(const Trainee& t) { trainees[t.id]=t; saveData(); }
    void addTrainer(const Trainer& t) { trainers[t.id]=t; saveData(); }
    void addManager(const Manager& m) { managers[m.id]=m; saveData(); }
    void addProgram(const TrainingProgram& p) { programs[p.id]=p; saveData(); }
    void removeProgram(const string& id) { if(programs.erase(id)) saveData(); }
    void addInquiry(const Inquiry& i) { inquiries[i.id]=i; saveData(); }
    void updateAppointment(const Appointment& a) { appointments[a.id]=a; saveData(); }

    void broadcastToBatch(const string& progId, const string& message) {
        string alert="[BATCH:"+progId+"] "+message;
        for (auto& it : trainees) {
            Trainee& st = it.second;
            const vector<string>& ep = st.enrolledPrograms;
            if(find(ep.begin(),ep.end(),progId)!=ep.end()) st.messages.push_back(alert);
        }
        saveData();
    }

    void login(const string& role) {
        for (int a=0;a<3;++a) {
            string id,pass; cout<<"\n--- "<<role<<" LOGIN ---\nID: ";cin>>id; cout<<"Password: ";cin>>pass;
            if(role=="ADMIN"  &&id=="admin"&&pass=="admin"){currentUser="admin";currentRole="ADMIN";return;}
            if(role=="MANAGER"){auto it=managers.find(id);if(it!=managers.end()&&it->second.password==pass){currentUser=id;currentRole="MANAGER";return;}}
            if(role=="TRAINER"){auto it=trainers.find(id);if(it!=trainers.end()&&it->second.password==pass){currentUser=id;currentRole="TRAINER";return;}}
            if(role=="TRAINEE"){auto it=trainees.find(id);if(it!=trainees.end()&&it->second.password==pass){currentUser=id;currentRole="TRAINEE";return;}}
            cout<<"[!] Wrong credentials. Left: "<<(2-a)<<"\n";
        }
        throw SecurityException("[LOCKED] Max attempts reached.\n");
    }

    void frontDeskMenu() {
        int ch; cout<<"\n--- FRONT DESK ---\n1. Submit Inquiry\n2. Request Appointment\nChoice: ";cin>>ch;
        if(ch==1){
            Inquiry i;i.id="INQ"+to_string(inquiries.size()+1);
            cout<<"Name: ";cin.ignore();getline(cin,i.name);
            cout<<"Contact: ";getline(cin,i.contact);
            cout<<"Question: ";getline(cin,i.question);
            i.status="Pending";
            addInquiry(i);
            cout<<"Submitted! ID: "<<i.id<<"\n";
        }
        else if(ch==2){
            Appointment a;a.id="APP"+to_string(appointments.size()+1);
            cout<<"Name: ";cin.ignore();getline(cin,a.name);
            cout<<"Purpose: ";getline(cin,a.purpose);
            a.date="Pending"; a.status="Pending";
            appointments[a.id]=a; saveData();
            cout<<"Requested! ID: "<<a.id<<"\n";
        }
    }

    void searchMenu() {
        int ch; cout<<"\n--- SEARCH ---\n1.Trainees 2.Trainers 3.Courses 4.Filter Placement\nChoice: ";cin>>ch;cin.ignore();
        if(ch==1){string q;cout<<"Query: ";getline(cin,q);cout<<"\n"<<left<<setw(12)<<"ID"<<setw(25)<<"Name"<<setw(10)<<"Dues"<<"Status\n"<<string(60,'-')<<"\n";bool f=false;
        for(const auto& it : trainees){const Trainee& tr=it.second;if(Utils::containsCI(tr.name,q)||Utils::containsCI(tr.id,q)){cout<<setw(12)<<tr.id<<setw(25)<<tr.name<<"$"<<setw(9)<<fixed<<setprecision(2)<<tr.feeBalance<<(tr.placementStatus.empty()?"None":tr.placementStatus)<<"\n";f=true;}}if(!f)cout<<"None found.\n";}
        else if(ch==2){string q;cout<<"Query: ";getline(cin,q);cout<<"\n"<<left<<setw(12)<<"ID"<<setw(25)<<"Name"<<"Domain\n"<<string(50,'-')<<"\n";bool f=false;
        for(const auto& it : trainers){const Trainer& tr=it.second;if(Utils::containsCI(tr.name,q)||Utils::containsCI(tr.domain,q)){cout<<setw(12)<<tr.id<<setw(25)<<tr.name<<tr.domain<<"\n";f=true;}}if(!f)cout<<"None found.\n";}
        else if(ch==3){string q;cout<<"Query: ";getline(cin,q);TrainingProgram::printCourseHeader();bool f=false;
        for(const auto& it : programs){const TrainingProgram& pg=it.second;if(Utils::containsCI(pg.name,q)||Utils::containsCI(pg.domain,q)){cout<<pg<<"\n";f=true;}}if(!f)cout<<"None found.\n";}
        else if(ch==4){cout<<"1.Placed 2.Eligible 3.Not Eligible\nChoice: ";int f;cin>>f;cout<<"\n"<<left<<setw(12)<<"ID"<<setw(25)<<"Name"<<"Status\n"<<string(55,'-')<<"\n";bool fd=false;
        for(const auto& it : trainees){const Trainee& tr=it.second;bool s=(f==1&&!tr.placementStatus.empty())||(f==2&&tr.placementEligible&&tr.placementStatus.empty())||(f==3&&!tr.placementEligible);
        if(s){cout<<setw(12)<<tr.id<<setw(25)<<tr.name<<(tr.placementStatus.empty()?(tr.placementEligible?"Eligible":"Not Eligible"):tr.placementStatus)<<"\n";fd=true;}}if(!fd)cout<<"None match.\n";}
    }

    void attendanceReport() {
        int ch; cout<<"\n--- ATTENDANCE ---\n1.By Trainee 2.By Course 3.Low Alert\nChoice: ";cin>>ch;
        if(ch==1){string sid;cout<<"Trainee ID: ";cin>>sid;auto it=trainees.find(sid);if(it==trainees.end()){cout<<"Not found.\n";return;}const Trainee& tr=it->second;cout<<"\nAttendance: "<<tr.name<<"\n";
        for(const string& pid:tr.enrolledPrograms){int att=tr.attendedClasses.count(pid)?tr.attendedClasses.at(pid):0;int tot=programs.count(pid)?programs.at(pid).totalClassesHeld:0;int pct=tot>0?att*100/tot:0;cout<<"  "<<left<<setw(32)<<programs[pid].name<<"  ";Utils::printProgressBar(att,tot);cout<<"  ("<<pct<<"%)"<<(pct<75&&tot>0?"  LOW":"")<<"\n";}}
        else if(ch==2){string pid;cout<<"Course ID: ";cin>>pid;if(!programs.count(pid)){cout<<"Not found.\n";return;}int tot=programs[pid].totalClassesHeld;cout<<"\nAttendance: "<<programs[pid].name<<" (classes:"<<tot<<")\n"<<left<<setw(12)<<"ID"<<setw(25)<<"Name"<<"Attendance\n"<<string(65,'-')<<"\n";bool f=false;
        for(const auto& it : trainees){const Trainee& tr=it.second;if(find(tr.enrolledPrograms.begin(),tr.enrolledPrograms.end(),pid)!=tr.enrolledPrograms.end()){int att=tr.attendedClasses.count(pid)?tr.attendedClasses.at(pid):0;int pct=tot>0?att*100/tot:0;cout<<setw(12)<<tr.id<<setw(25)<<tr.name;Utils::printProgressBar(att,tot,20);cout<<" ("<<pct<<"%)"<<(pct<75&&tot>0?" LOW":"")<<"\n";f=true;}}if(!f)cout<<"No trainees enrolled.\n";}
        else if(ch==3){cout<<"\nLow Attendance (<75%):\n"<<left<<setw(12)<<"ID"<<setw(25)<<"Name"<<setw(32)<<"Course"<<"Attendance\n"<<string(82,'-')<<"\n";bool f=false;
        for(const auto& it : trainees){const Trainee& tr=it.second;for(const string& pid:tr.enrolledPrograms){int tot=programs.count(pid)?programs.at(pid).totalClassesHeld:0;int att=tr.attendedClasses.count(pid)?tr.attendedClasses.at(pid):0;int pct=tot>0?att*100/tot:0;if(pct<75&&tot>0){cout<<setw(12)<<tr.id<<setw(25)<<tr.name<<setw(32)<<(programs.count(pid)?programs.at(pid).name:pid)<<att<<"/"<<tot<<" ("<<pct<<"%)\n";f=true;}}}if(!f)cout<<"All >=75%.\n";}
    }

    void feeTrackingMenu() {
        int ch; cout<<"\n--- FEE TRACKING ---\n1.All Dues 2.History 3.Defaulters 4.Record Payment\nChoice: ";cin>>ch;
        if(ch==1){float g=0;cout<<"\n"<<left<<setw(12)<<"ID"<<setw(25)<<"Name"<<setw(12)<<"Dues"<<"Paid\n"<<string(60,'-')<<"\n";
        for(const auto& it : trainees){const Trainee& tr=it.second;cout<<setw(12)<<tr.id<<setw(25)<<tr.name<<"$"<<setw(11)<<fixed<<setprecision(2)<<tr.feeBalance<<"$"<<tr.totalPaid<<"\n";g+=tr.feeBalance;}cout<<string(60,'-')<<"\nOUTSTANDING: $"<<fixed<<setprecision(2)<<g<<"\n";}
        else if(ch==2){string sid;cout<<"Trainee ID: ";cin>>sid;auto it=trainees.find(sid);if(it==trainees.end()){cout<<"Not found.\n";return;}const Trainee& tr=it->second;cout<<"\nStatement: "<<tr.name<<"\nDues: $"<<fixed<<setprecision(2)<<tr.feeBalance<<"  Paid: $"<<tr.totalPaid<<"\n";int i=1;for(const string& r:tr.paymentHistory){size_t s=r.find('|');cout<<"  "<<i++<<". "<<(s!=string::npos?r.substr(0,s):r)<<"  on  "<<(s!=string::npos?r.substr(s+1):"N/A")<<"\n";}}
        else if(ch==3){float t;cout<<"Threshold $: ";cin>>t;bool f=false;
        for(const auto& it : trainees){const Trainee& tr=it.second;if(tr.feeBalance>t){cout<<"  "<<tr.id<<" | "<<tr.name<<" | $"<<fixed<<setprecision(2)<<tr.feeBalance<<"\n";f=true;}}if(!f)cout<<"No defaulters.\n";}
        else if(ch==4){string sid;cout<<"Trainee ID: ";cin>>sid;auto it=trainees.find(sid);if(it==trainees.end()){cout<<"Not found.\n";return;}Trainee& tr=it->second;cout<<"Dues: $"<<fixed<<setprecision(2)<<tr.feeBalance<<"\nAmount: $";float p;cin>>p;tr-=p;saveData();cout<<"Done. Remaining: $"<<fixed<<setprecision(2)<<tr.feeBalance<<"\n";}
    }

    void placementManagementMenu() {
        int ch;
        cout << "\n========================================\n"
             << "        PLACEMENT MANAGEMENT\n"
             << "========================================\n"
             << " 1. View Placement Pool\n"
             << " 2. Run Placement Drive\n"
             << " 3. Manage Hiring Partners\n"
             << " 4. Placement Dashboard\n"
             << " 5. View Trainee Placement Profile\n"
             << " 6. Set Mock Interview Score\n"
             << " 7. Export Placement Report\n"
             << " 0. Back\nChoice: ";
        cin >> ch;

        if (ch == 1) {
            cout << "\n--- PLACEMENT POOL ---\n";
            if (placementPool.empty()) { cout << "Pool is empty.\n"; return; }
            cout << left << setw(12)<<"ID" << setw(22)<<"Name"
                 << setw(22)<<"Pref. Company" << setw(20)<<"Pref. Role"
                 << setw(7)<<"Score" << "Status\n" << string(90,'-') << "\n";
            for (const string& tid : placementPool) {
                auto it=trainees.find(tid); if(it==trainees.end()) continue;
                const Trainee& tr=it->second;
                cout << setw(12)<<tr.id << setw(22)<<tr.name
                     << setw(22)<<(tr.preferredCompany.empty()?"—":tr.preferredCompany)
                     << setw(20)<<(tr.preferredRole.empty()?"—":tr.preferredRole)
                     << setw(7) <<tr.placementScore
                     << (tr.placementStatus.empty()?"Awaiting":tr.placementStatus) << "\n";
            }
        }
        else if (ch == 2) {
            cout << "\n--- PLACEMENT DRIVE ---\n";
            if (placementPool.empty()) { cout << "Pool is empty.\n"; return; }
            for (auto it=placementPool.begin(); it!=placementPool.end(); ) {
                auto tit=trainees.find(*it);
                if(tit==trainees.end()){ it=placementPool.erase(it); saveData(); continue; }
                Trainee& tr=tit->second;
                cout << "\n  Candidate : " << tr.name << " (" << tr.id << ")\n"
                     << "  Preferred : " << (tr.preferredCompany.empty()?"N/A":tr.preferredCompany)
                     << "  |  Role: "    << (tr.preferredRole.empty()?"N/A":tr.preferredRole) << "\n"
                     << "  Score     : " << tr.placementScore << "/100\n"
                     << "  Resume    : " << (tr.resume.empty()?"Not provided":tr.resume) << "\n"
                     << "  1. Make Offer   2. Reject   3. Skip\n  Choice: ";
                int d; cin>>d;
                if (d == 1) {
                    PlacementRecord pr;
                    cout<<"  Company    : ";cin.ignore();getline(cin,pr.company);
                    cout<<"  Role       : ";getline(cin,pr.role);
                    cout<<"  Package    : ";getline(cin,pr.package);
                    cout<<"  Offer Date : ";getline(cin,pr.offerDate);
                    cout<<"  Join Date  : ";getline(cin,pr.joiningDate);
                    pr.status="Offered";
                    tr.placementHistory.push_back(pr);
                    tr.placementStatus="Offered by "+pr.company+" ("+pr.package+")";
                    tr.messages.push_back("Offer from "+pr.company+" | "+pr.role+" | "+pr.package);
                    for(const string& pid:tr.enrolledPrograms) {
                        if(programs.count(pid)){
                            TrainingProgram& pg=programs[pid];
                            pg.placedCount++;
                            float pkg=0;
                            stringstream ss(pr.package); ss>>pkg;
                            pg.avgPackage=((pg.avgPackage*(pg.placedCount-1))+pkg)/pg.placedCount;
                            if(pg.topRecruiter.empty()) pg.topRecruiter=pr.company;
                        }
                    }
                    it=placementPool.erase(it);
                    saveData();
                    cout<<"  Offer recorded!\n";
                } else if (d==2) {
                    PlacementRecord pr; pr.company=tr.preferredCompany; pr.role=tr.preferredRole;
                    pr.offerDate=Utils::now(); pr.status="Rejected";
                    tr.placementHistory.push_back(pr);
                    tr.messages.push_back("Update: application at "+(tr.preferredCompany.empty()?"company":tr.preferredCompany)+" not proceeded. Keep applying!");
                    ++it;
                    saveData();
                    cout<<"  Rejection noted.\n";
                } else { ++it; }
            }
        }
        else if (ch == 3) {
            cout<<"\nPartners: ";for(const string& hp:hiringPartners)cout<<hp<<"  ";
            cout<<"\n1.Add 2.Remove\nChoice: ";int s;cin>>s;
            if(s==1){string hp;cout<<"Company: ";cin.ignore();getline(cin,hp);if(find(hiringPartners.begin(),hiringPartners.end(),hp)==hiringPartners.end()){hiringPartners.push_back(hp);saveData();cout<<"Added!\n";}else cout<<"Already listed.\n";}
            else if(s==2){string hp;cout<<"Remove: ";cin.ignore();getline(cin,hp);vector<string>::iterator it=find(hiringPartners.begin(),hiringPartners.end(),hp);if(it!=hiringPartners.end()){hiringPartners.erase(it);saveData();cout<<"Removed.\n";}else cout<<"Not found.\n";}
        }
        else if (ch == 4) {
            int eligible=0,applied=0,placed=0;
            for(const auto& it : trainees){const Trainee& tr=it.second;if(tr.placementEligible)eligible++;if(!tr.placementHistory.empty())applied++;if(!tr.placementStatus.empty())placed++;}
            cout<<"\n========================================\n"
                <<"         PLACEMENT DASHBOARD\n"
                <<"========================================\n"
                <<"  Total Trainees         : "<<trainees.size()<<"\n"
                <<"  Eligible for Placement : "<<eligible<<"\n"
                <<"  Applied / In History   : "<<applied<<"\n"
                <<"  Currently Placed       : "<<placed<<"\n\n"
                <<"  Course-wise Placement:\n"
                <<"  "<<left<<setw(35)<<"Course"<<setw(8)<<"Placed"<<setw(10)<<"Avg Pkg"<<"Top Recruiter\n"
                <<"  "<<string(68,'-')<<"\n";
            for(const auto& it : programs){
                const TrainingProgram& pg=it.second;
                cout<<"  "<<setw(35)<<pg.name<<setw(8)<<pg.placedCount
                    <<setw(10)<<(pg.avgPackage>0?to_string((int)pg.avgPackage)+" LPA":"N/A")
                    <<(pg.topRecruiter.empty()?"N/A":pg.topRecruiter)<<"\n";
            }
            cout<<"\n  Hiring Partners: ";for(const string& hp:hiringPartners)cout<<hp<<"  ";cout<<"\n";
        }
        else if (ch == 5) {
            string sid;cout<<"Trainee ID: ";cin>>sid;
            auto it=trainees.find(sid);if(it==trainees.end()){cout<<"Not found.\n";return;}
            const Trainee& tr=it->second;
            cout<<"\n========================================\n"
                <<"  PLACEMENT PROFILE: "<<tr.name<<"\n"
                <<"========================================\n"
                <<"  Eligible       : "<<(tr.placementEligible?"Yes":"No")<<"\n"
                <<"  Mock Score     : "<<tr.placementScore<<"/100\n"
                <<"  Preferred Co.  : "<<(tr.preferredCompany.empty()?"N/A":tr.preferredCompany)<<"\n"
                <<"  Preferred Role : "<<(tr.preferredRole.empty()?"N/A":tr.preferredRole)<<"\n"
                <<"  Resume / Link  : "<<(tr.resume.empty()?"N/A":tr.resume)<<"\n"
                <<"  Status         : "<<(tr.placementStatus.empty()?"Not placed":tr.placementStatus)<<"\n\n";
            if(tr.placementHistory.empty()){cout<<"  No placement history.\n";}
            else{
                cout<<"  History:\n  "<<left<<setw(4)<<"#"<<setw(20)<<"Company"<<setw(20)<<"Role"<<setw(10)<<"Package"<<setw(14)<<"Offer Date"<<"Status\n  "<<string(73,'-')<<"\n";
                int i=1;for(const auto& pr:tr.placementHistory)cout<<"  "<<setw(4)<<i++<<setw(20)<<pr.company<<setw(20)<<pr.role<<setw(10)<<pr.package<<setw(14)<<pr.offerDate<<pr.status<<"\n";
            }
        }
        else if (ch == 6) {
            string sid;cout<<"Trainee ID: ";cin>>sid;
            auto it=trainees.find(sid);if(it==trainees.end()){cout<<"Not found.\n";return;}
            int score;cout<<"Mock Score (0-100): ";cin>>score;
            it->second.placementScore=max(0,min(100,score));
            saveData();
            cout<<"Updated: "<<it->second.placementScore<<"/100\n";
        }
        else if (ch == 7) {
            ofstream rpt("placement_reportdetails.txt");
            rpt<<"################################################################################\n"
               <<"#         MODERN TECH TRAINING INSTITUTE - PLACEMENT REPORT                   #\n"
               <<"#  Generated: "<<left<<setw(65)<<Utils::now()<<"#\n"
               <<"################################################################################\n\n";

            int placed=0; float totalPkg=0; int pkgCount=0;
            for(const auto& it : trainees){
                const Trainee& tr=it.second;
                if(tr.placementHistory.empty()&&tr.placementStatus.empty()) continue;
                rpt<<"  Trainee  : "<<tr.name<<" ("<<tr.id<<")\n"
                   <<"  Eligible : "<<(tr.placementEligible?"Yes":"No")
                   <<"  |  Score: "<<tr.placementScore<<"/100\n"
                   <<"  Status   : "<<(tr.placementStatus.empty()?"Not placed":tr.placementStatus)<<"\n";
                if(!tr.placementHistory.empty()){
                    rpt<<"  History  :\n"
                       <<"    "<<left<<setw(22)<<"Company"<<setw(22)<<"Role"<<setw(12)<<"Package"<<setw(14)<<"Offer Date"<<"Status\n"
                       <<"    "<<string(74,'-')<<"\n";
                    for(const auto& pr:tr.placementHistory){
                        rpt<<"    "<<setw(22)<<pr.company<<setw(22)<<pr.role<<setw(12)<<pr.package<<setw(14)<<pr.offerDate<<pr.status<<"\n";
                        if(pr.status=="Offered"||pr.status=="Placed"){placed++;stringstream ss(pr.package); float pkg; ss>>pkg; totalPkg+=pkg; pkgCount++;}
                    }
                }
                rpt<<"  "<<string(70,'-')<<"\n\n";
            }
            rpt<<"################################################################################\n"
               <<"#  SUMMARY                                                                     #\n"
               <<"################################################################################\n"
               <<"  Total Placed   : "<<placed<<"\n"
               <<"  Average Package: "<<(pkgCount>0?to_string(totalPkg/pkgCount)+" LPA":"N/A")<<"\n"
               <<"  Hiring Partners: ";
            for(const string& hp:hiringPartners) rpt<<hp<<"  ";
            rpt<<"\n\n################################################################################\n"
               <<"#  END OF REPORT                                                               #\n"
               <<"################################################################################\n";
            rpt.close();
            cout<<"Exported to: placement_report.txt\n";
        }
    }

    void adminMenu() {
        int c;
        do {
            cout<<"\n--- ADMIN ---\n 1.Users  2.Add Manager  3.Courses  4.Assign Trainer  5.Set Salary\n"
                <<" 6.Admissions  7.Appointments  8.Search  9.Attendance  10.Fees\n"
                <<"11.Record Class  12.Placement Management  0.Logout\nChoice: ";
            cin>>c;
            if(c==1){cout<<"\n[TRAINEES]\n";for(const auto& it : trainees){const Trainee& t=it.second;cout<<"  "<<t.id<<" | "<<t.name<<" | $"<<fixed<<setprecision(2)<<t.feeBalance<<"\n";}cout<<"\n[TRAINERS]\n";for(const auto& it : trainers){const Trainer& t=it.second;cout<<"  "<<t.id<<" | "<<t.name<<" | "<<t.domain<<"\n";}}
            else if(c==2){Manager m;cout<<"ID: ";cin>>m.id;cout<<"Pw: ";cin>>m.password;cout<<"Name: ";cin.ignore();getline(cin,m.name);cout<<"Dept: ";getline(cin,m.domain);addManager(m);cout<<"Created!\n";}
            else if(c==3){int cc;cout<<"1.Add 2.Delete\nChoice: ";cin>>cc;
            if(cc==1){TrainingProgram p;cout<<"ID: ";cin>>p.id;cout<<"Name: ";cin.ignore();getline(cin,p.name);cout<<"Domain: ";getline(cin,p.domain);cout<<"Schedule: ";getline(cin,p.scheduleTime);cout<<"Weeks: ";cin>>p.durationWeeks;cout<<"MaxSeats: ";cin>>p.maxSeats;cout<<"Fee: ";cin>>p.feeAmount;cout<<"Topics: ";cin>>p.totalTopics;cout<<"ExamMins: ";cin>>p.examDurationMins;cout<<"ExamQ: ";cin.ignore();getline(cin,p.certExamQuestion);cout<<"ExamA: ";getline(cin,p.certExamAnswer);p.enrolledSeats=0;p.completedTopics=0;p.totalClassesHeld=0;addProgram(p);cout<<"Published!\n";}else if(cc==2){string did;cout<<"ID: ";cin>>did;removeProgram(did);cout<<"Deleted.\n";}}
            else if(c==4){string pid,tid;cout<<"Course ID: ";cin>>pid;cout<<"Trainer ID: ";cin>>tid;
            if(programs.count(pid)&&trainers.count(tid)){
                programs[pid].assignedTrainerId=tid;
                vector<string>& b=trainers[tid].activeBatches;
                if(find(b.begin(),b.end(),pid)==b.end()) b.push_back(pid);
                saveData();
                cout<<"Assigned!\n";
            }else cout<<"Invalid.\n";}
            else if(c==5){string tid;cout<<"Trainer ID: ";cin>>tid;auto it=trainers.find(tid);if(it!=trainers.end()){cout<<"Salary: $";cin>>it->second.salary;saveData();cout<<"Done!\n";}else cout<<"Not found.\n";}
            else if(c==6){bool has=false;
            for(auto& it : trainees){
                Trainee& tr=it.second;
                for(vector<string>::iterator itp=tr.pendingPrograms.begin();itp!=tr.pendingPrograms.end();){
                    if(!programs.count(*itp)){itp=tr.pendingPrograms.erase(itp);continue;}
                    has=true;
                    cout<<tr.name<<" -> "<<programs[*itp].name<<"  [Y/N/S]: ";char d;cin>>d;
                    if(d=='Y'||d=='y'){
                        tr.enrolledPrograms.push_back(*itp);
                        programs[*itp].enrolledSeats++;
                        tr+=programs[*itp].feeAmount;
                        tr.messages.push_back("Admitted: "+*itp);
                        itp=tr.pendingPrograms.erase(itp);
                        saveData();
                        cout<<"Approved!\n";
                    }else if(d=='N'||d=='n'){
                        itp=tr.pendingPrograms.erase(itp);
                        saveData();
                        cout<<"Rejected.\n";
                    }else ++itp;
                }
            }if(!has)cout<<"No pending.\n";}
            else if(c==7){bool f=false;
            for(auto& it : appointments){
                Appointment& a=it.second;
                if(a.status=="Pending"){
                    f=true;cout<<a.id<<" | "<<a.name<<" | "<<a.purpose<<"\nApprove? Y/N: ";char d;cin>>d;
                    if(d=='Y'||d=='y'){cout<<"Date: ";string dt;cin>>dt;a.date=dt;a.status="Approved";updateAppointment(a);cout<<"Done!\n";}
                    else if(d=='N'||d=='n'){a.status="Rejected";updateAppointment(a);}
                }
            }if(!f)cout<<"No pending.\n";}
            else if(c==8)  searchMenu();
            else if(c==9)  attendanceReport();
            else if(c==10) feeTrackingMenu();
            else if(c==11){string pid;cout<<"Program ID: ";cin>>pid;if(!programs.count(pid)){cout<<"Not found.\n";}else{
                programs[pid].totalClassesHeld++;
                cout<<"Class #"<<programs[pid].totalClassesHeld<<".\nAttendees (blank=stop):\n";cin.ignore();
                while(true){string sid;getline(cin,sid);if(sid.empty())break;
                if(trainees.count(sid)){trainees[sid].attendedClasses[pid]++;saveData();cout<<"  OK: "<<trainees[sid].name<<"\n";}
                else cout<<"  Not found.\n";}}}
            else if(c==12) placementManagementMenu();
        } while(c!=0);
    }

    // FIXED: Corrected managerMenu function
    void managerMenu() {
        Manager& m = managers[currentUser];
        User* u = &m;
        u->printRole();
        int c;
        do {
            cout << "\n 1.Register Trainee  2.Register Trainer  3.Inquiries\n"
                 << " 4.Placement Management  5.Search  6.Fees  7.Attendance\n"
                 << " 0.Logout\nChoice: ";
            cin >> c;
            if(c == 1) {
                Trainee s;
                cout << "ID: "; cin >> s.id;
                if(trainees.count(s.id)) { cout << "Exists!\n"; continue; }
                cout << "Pw: "; cin >> s.password;
                cout << "Name: "; cin.ignore(); getline(cin, s.name);
                s.feeBalance = 0; s.totalPaid = 0; s.placementEligible = false;
                addTrainee(s);
                cout << "Registered!\n";
            }
            else if(c == 2) {
                Trainer t;
                cout << "ID: "; cin >> t.id;
                if(trainers.count(t.id)) { cout << "Exists!\n"; continue; }
                cout << "Pw: "; cin >> t.password;
                cout << "Name: "; cin.ignore(); getline(cin, t.name);
                cout << "Domain: "; getline(cin, t.domain);
                t.salary = 0;
                addTrainer(t);
                cout << "Registered!\n";
            }
            else if(c == 3) {
                bool f = false;
                for(auto it = inquiries.begin(); it != inquiries.end(); ) {
                    if(it->second.status == "Pending") {
                        f = true;
                        cout << "ID: " << it->second.id << " | " << it->second.name
                             << "\nQ: " << it->second.question
                             << "\n1.Resolve 2.Convert 3.Skip: ";
                        int r; cin >> r;
                        if(r == 1) {
                            it->second.status = "Resolved";
                            saveData();  // Fixed: Save inquiry update directly
                            ++it;
                        }
                        else if(r == 2) {
                            Trainee s;
                            s.id = "TRN" + it->second.id;
                            s.password = "12345";
                            s.name = it->second.name;
                            s.feeBalance = 0;
                            s.totalPaid = 0;
                            s.placementEligible = false;
                            addTrainee(s);
                            cout << "Converted! ID: " << s.id << "\n";
                            it = inquiries.erase(it);
                            saveData();
                        }
                        else {
                            ++it;
                        }
                    }
                    else {
                        ++it;
                    }
                }
                if(!f) cout << "No pending.\n";
            }
            else if(c == 4) placementManagementMenu();
            else if(c == 5) searchMenu();
            else if(c == 6) feeTrackingMenu();
            else if(c == 7) attendanceReport();
        } while(c != 0);
    }

    void trainerMenu() {
        Trainer& t=trainers[currentUser];
        User* u=&t;
        u->printRole();
        int c;
        do {
            cout<<"\n1.Courses  2.Upload Material  3.Grade  4.Mark Attendance  5.Update Progress  0.Logout\nChoice: ";cin>>c;
            if(c==1){if(t.activeBatches.empty()){cout<<"No courses.\n";continue;}TrainingProgram::printCourseHeader();for(const string& pid:t.activeBatches){if(programs.count(pid))cout<<programs[pid]<<"\n";else cout<<"[Not found: "<<pid<<"]\n";}}
            else if(c==2){string pid,mod;cout<<"Program ID: ";cin>>pid;if(!programs.count(pid)){cout<<"Not found.\n";continue;}cout<<"Material: ";cin.ignore();getline(cin,mod);programs[pid].modules.push_back(mod);saveData();broadcastToBatch(pid,"New material: "+mod);cout<<"Uploaded!\n";}
            else if(c==3){string sid,pid,g;cout<<"Trainee ID: ";cin>>sid;cout<<"Program ID: ";cin>>pid;cout<<"Grade: ";cin>>g;if(!trainees.count(sid)||!programs.count(pid)){cout<<"Not found.\n";continue;}trainees[sid].projectGrades[pid]=g;trainees[sid].messages.push_back("Grade for "+pid+": "+g);saveData();cout<<"Graded!\n";}
            else if(c==4){string pid;cout<<"Program ID: ";cin>>pid;if(!programs.count(pid)){cout<<"Not found.\n";continue;}programs[pid].totalClassesHeld++;cout<<"Class #"<<programs[pid].totalClassesHeld<<".\nAttendees:\n";cin.ignore();while(true){string sid;getline(cin,sid);if(sid.empty())break;if(trainees.count(sid)){trainees[sid].attendedClasses[pid]++;saveData();cout<<"  OK: "<<trainees[sid].name<<"\n";}else cout<<"  Not found.\n";}}
            else if(c==5){string pid;cout<<"Program ID: ";cin>>pid;if(!programs.count(pid)){cout<<"Not found.\n";continue;}int done;cout<<"Completed topics: ";cin>>done;programs[pid].completedTopics=done;saveData();cout<<"Updated: "<<(programs[pid].totalTopics>0?done*100/programs[pid].totalTopics:0)<<"%\n";}
        } while(c!=0);
    }

    void traineeMenu() {
        Trainee& s=trainees[currentUser];
        User* u=&s;
        u->printRole();
        if(!s.messages.empty()){cout<<"NOTIFICATIONS:\n";Utils::printVector(s.messages,"");s.messages.clear();saveData();}
        int c;
        do {
            cout<<"\n1.Browse & Enroll  2.My Progress  3.Certification Exam  4.Placement Portal  5.Fee Statement  0.Logout\nChoice: ";cin>>c;
            if(c==1){cout<<"\n--- COURSES ---\n";TrainingProgram::printCourseHeader();for(const auto& it : programs){cout<<it.second<<"\n";}string pid;cout<<"\nCourse ID (0=cancel): ";cin>>pid;if(pid=="0")continue;if(!programs.count(pid)){cout<<"Invalid.\n";continue;}if(find(s.pendingPrograms.begin(),s.pendingPrograms.end(),pid)!=s.pendingPrograms.end()||find(s.enrolledPrograms.begin(),s.enrolledPrograms.end(),pid)!=s.enrolledPrograms.end()){cout<<"Already applied/enrolled.\n";continue;}if(programs[pid].enrolledSeats>=programs[pid].maxSeats){cout<<"Full.\n";continue;}s.pendingPrograms.push_back(pid);saveData();cout<<"Applied! Pending approval.\n";}
            else if(c==2){if(s.enrolledPrograms.empty()){cout<<"Not enrolled yet.\n";continue;}for(const string& pid:s.enrolledPrograms){if(!programs.count(pid))continue;const TrainingProgram& pg=programs[pid];int pct=(pg.totalTopics>0)?pg.completedTopics*100/pg.totalTopics:0;int att=s.attendedClasses.count(pid)?s.attendedClasses.at(pid):0;int aPct=(pg.totalClassesHeld>0)?att*100/pg.totalClassesHeld:0;cout<<"\n+ "<<pg.name<<"\n  Progress: "<<pct<<"% ";Utils::printProgressBar(pg.completedTopics,pg.totalTopics,20);cout<<"\n  Attend:   "<<aPct<<"% ";Utils::printProgressBar(att,pg.totalClassesHeld,20);cout<<"\n  Grade: "<<(s.projectGrades.count(pid)?s.projectGrades.at(pid):"Pending")<<"\n  Materials:\n";Utils::printVector(pg.modules,"None yet.");}}
            else if(c==3){string pid;cout<<"Course ID: ";cin>>pid;if(!programs.count(pid)){cout<<"Not found.\n";continue;}const TrainingProgram& pg=programs[pid];if(pg.certExamQuestion.empty()){cout<<"No exam.\n";continue;}if(find(s.enrolledPrograms.begin(),s.enrolledPrograms.end(),pid)==s.enrolledPrograms.end()){cout<<"Must be enrolled.\n";continue;}cout<<"\n--- EXAM ("<<pg.examDurationMins<<" mins) ---\nQ: "<<pg.certExamQuestion<<"\nAnswer: ";auto start=chrono::steady_clock::now();string ans;cin.ignore();getline(cin,ans);auto el=chrono::duration_cast<chrono::minutes>(chrono::steady_clock::now()-start).count();if(el>pg.examDurationMins){cout<<"FAILED: Timeout.\n";}else{string al=ans,cl=pg.certExamAnswer;transform(al.begin(),al.end(),al.begin(),::tolower);transform(cl.begin(),cl.end(),cl.begin(),::tolower);if(al==cl){s.placementEligible=true;saveData();cout<<"PASSED! Now placement eligible.\n";}else cout<<"FAILED.\n";}}
            else if(c==4){
                cout<<"\n========================================\n"
                    <<"      YOUR PLACEMENT PORTAL\n"
                    <<"========================================\n"
                    <<"  Eligible    : "<<(s.placementEligible?"YES":"NO")<<"\n"
                    <<"  Mock Score  : "<<s.placementScore<<"/100\n"
                    <<"  Status      : "<<(s.placementStatus.empty()?"Not placed":s.placementStatus)<<"\n"
                    <<"  Pref Company: "<<(s.preferredCompany.empty()?"Not set":s.preferredCompany)<<"\n"
                    <<"  Pref Role   : "<<(s.preferredRole.empty()?"Not set":s.preferredRole)<<"\n"
                    <<"  Resume/Link : "<<(s.resume.empty()?"Not set":s.resume)<<"\n";
                if(!s.placementEligible){cout<<"  Pass a cert exam to unlock placement.\n";continue;}
                cout<<"\n  1.Update Profile & Apply  2.View History  3.Hiring Partners  0.Back\n  Choice: ";
                int pc;cin>>pc;
                if(pc==1){cout<<"Preferred Company: ";cin.ignore();getline(cin,s.preferredCompany);cout<<"Preferred Role: ";getline(cin,s.preferredRole);cout<<"Resume/Link: ";getline(cin,s.resume);if(find(placementPool.begin(),placementPool.end(),s.id)==placementPool.end())placementPool.push_back(s.id);saveData();cout<<"Profile saved & added to placement pool!\n";}
                else if(pc==2){if(s.placementHistory.empty()){cout<<"No history.\n";}else{cout<<"\n  "<<left<<setw(4)<<"#"<<setw(20)<<"Company"<<setw(20)<<"Role"<<setw(12)<<"Package"<<setw(14)<<"Date"<<"Status\n  "<<string(73,'-')<<"\n";int i=1;for(const auto& pr:s.placementHistory)cout<<"  "<<setw(4)<<i++<<setw(20)<<pr.company<<setw(20)<<pr.role<<setw(12)<<pr.package<<setw(14)<<pr.offerDate<<pr.status<<"\n";}}
                else if(pc==3){cout<<"Partners: ";for(const string& hp:hiringPartners)cout<<hp<<"  ";cout<<"\n";}
            }
            else if(c==5){cout<<"\n--- FEE STATEMENT ---\nDues: $"<<fixed<<setprecision(2)<<s.feeBalance<<"  Paid: $"<<s.totalPaid<<"\n";int i=1;for(const string& r:s.paymentHistory){size_t sp=r.find('|');cout<<"  "<<i++<<". "<<(sp!=string::npos?r.substr(0,sp):r)<<"  on  "<<(sp!=string::npos?r.substr(sp+1):"N/A")<<"\n";}if(s.feeBalance<=0){cout<<"No dues.\n";continue;}cout<<"Pay ($, 0=skip): ";float pay;cin>>pay;if(pay>0){s-=pay;saveData();cout<<"Paid. Remaining: $"<<fixed<<setprecision(2)<<s.feeBalance<<"\n";}}
        } while(c!=0);
    }

public:
    TrainingInstitute() {
        loadData();
        if (programs.empty()) {

TrainingProgram p3; p3.id="AI301"; p3.name="AI & Machine Learning"; p3.domain="AI";
p3.scheduleTime="Mon-Wed 6:00 PM"; p3.durationWeeks=20; p3.maxSeats=35;
p3.enrolledSeats=0; p3.feeAmount=2000.0f; p3.totalTopics=30; p3.completedTopics=0;
p3.totalClassesHeld=0; p3.certExamQuestion="What is supervised learning?";
p3.certExamAnswer="learning with labeled data"; p3.examDurationMins=5;
programs[p3.id]=p3;

TrainingProgram p4; p4.id="CY101"; p4.name="Cyber Security Basics"; p4.domain="Security";
p4.scheduleTime="Tue-Thu 5:00 PM"; p4.durationWeeks=10; p4.maxSeats=45;
p4.enrolledSeats=0; p4.feeAmount=1000.0f; p4.totalTopics=18; p4.completedTopics=0;
p4.totalClassesHeld=0; p4.certExamQuestion="What is phishing?";
p4.certExamAnswer="fraudulent attempt to steal data"; p4.examDurationMins=5;
programs[p4.id]=p4;

TrainingProgram p5; p5.id="CC101"; p5.name="Cloud Computing AWS"; p5.domain="Cloud";
p5.scheduleTime="Weekends 2:00 PM"; p5.durationWeeks=12; p5.maxSeats=40;
p5.enrolledSeats=0; p5.feeAmount=1800.0f; p5.totalTopics=22; p5.completedTopics=0;
p5.totalClassesHeld=0; p5.certExamQuestion="What is AWS?";
p5.certExamAnswer="amazon web services"; p5.examDurationMins=5;
programs[p5.id]=p5;

TrainingProgram p6; p6.id="APP101"; p6.name="Android App Development"; p6.domain="Mobile";
p6.scheduleTime="Mon-Fri 7:00 PM"; p6.durationWeeks=14; p6.maxSeats=50;
p6.enrolledSeats=0; p6.feeAmount=1400.0f; p6.totalTopics=24; p6.completedTopics=0;
p6.totalClassesHeld=0; p6.certExamQuestion="Language for Android?";
p6.certExamAnswer="java or kotlin"; p6.examDurationMins=5;
programs[p6.id]=p6;

TrainingProgram p7; p7.id="UI201"; p7.name="UI/UX Design Mastery"; p7.domain="Design";
p7.scheduleTime="Wed-Fri 4:00 PM"; p7.durationWeeks=8; p7.maxSeats=30;
p7.enrolledSeats=0; p7.feeAmount=900.0f; p7.totalTopics=15; p7.completedTopics=0;
p7.totalClassesHeld=0; p7.certExamQuestion="What is UX?";
p7.certExamAnswer="user experience"; p7.examDurationMins=5;
programs[p7.id]=p7;

TrainingProgram p8; p8.id="DEV101"; p8.name="DevOps Fundamentals"; p8.domain="DevOps";
p8.scheduleTime="Tue-Thu 6:30 PM"; p8.durationWeeks=10; p8.maxSeats=35;
p8.enrolledSeats=0; p8.feeAmount=1600.0f; p8.totalTopics=20; p8.completedTopics=0;
p8.totalClassesHeld=0; p8.certExamQuestion="What is CI/CD?";
p8.certExamAnswer="continuous integration continuous deployment"; p8.examDurationMins=5;
programs[p8.id]=p8;

TrainingProgram p9; p9.id="NET101"; p9.name="Computer Networks"; p9.domain="Networking";
p9.scheduleTime="Mon-Wed 3:00 PM"; p9.durationWeeks=9; p9.maxSeats=40;
p9.enrolledSeats=0; p9.feeAmount=1100.0f; p9.totalTopics=17; p9.completedTopics=0;
p9.totalClassesHeld=0; p9.certExamQuestion="What is IP?";
p9.certExamAnswer="internet protocol"; p9.examDurationMins=5;
programs[p9.id]=p9;

TrainingProgram p10; p10.id="JAVA101"; p10.name="Java Programming"; p10.domain="Programming";
p10.scheduleTime="Mon-Fri 11:00 AM"; p10.durationWeeks=12; p10.maxSeats=60;
p10.enrolledSeats=0; p10.feeAmount=1200.0f; p10.totalTopics=25; p10.completedTopics=0;
p10.totalClassesHeld=0; p10.certExamQuestion="What is JVM?";
p10.certExamAnswer="java virtual machine"; p10.examDurationMins=5;
programs[p10.id]=p10;

TrainingProgram p11; p11.id="CPP101"; p11.name="C++ Mastery"; p11.domain="Programming";
p11.scheduleTime="Tue-Thu 10:00 AM"; p11.durationWeeks=10; p11.maxSeats=50;
p11.enrolledSeats=0; p11.feeAmount=1000.0f; p11.totalTopics=20; p11.completedTopics=0;
p11.totalClassesHeld=0; p11.certExamQuestion="What is OOP?";
p11.certExamAnswer="object oriented programming"; p11.examDurationMins=5;
programs[p11.id]=p11;

TrainingProgram p12; p12.id="PY101"; p12.name="Python Programming"; p12.domain="Programming";
p12.scheduleTime="Weekends 11:00 AM"; p12.durationWeeks=8; p12.maxSeats=60;
p12.enrolledSeats=0; p12.feeAmount=900.0f; p12.totalTopics=18; p12.completedTopics=0;
p12.totalClassesHeld=0; p12.certExamQuestion="What is Python?";
p12.certExamAnswer="programming language"; p12.examDurationMins=5;
programs[p12.id]=p12;

TrainingProgram p13; p13.id="SQL101"; p13.name="SQL & Databases"; p13.domain="Database";
p13.scheduleTime="Mon-Wed 2:00 PM"; p13.durationWeeks=6; p13.maxSeats=45;
p13.enrolledSeats=0; p13.feeAmount=800.0f; p13.totalTopics=12; p13.completedTopics=0;
p13.totalClassesHeld=0; p13.certExamQuestion="What is SQL?";
p13.certExamAnswer="structured query language"; p13.examDurationMins=5;
programs[p13.id]=p13;

TrainingProgram p14; p14.id="BLK101"; p14.name="Blockchain Basics"; p14.domain="Blockchain";
p14.scheduleTime="Fri-Sun 5:00 PM"; p14.durationWeeks=8; p14.maxSeats=30;
p14.enrolledSeats=0; p14.feeAmount=1300.0f; p14.totalTopics=15; p14.completedTopics=0;
p14.totalClassesHeld=0; p14.certExamQuestion="What is blockchain?";
p14.certExamAnswer="distributed ledger technology"; p14.examDurationMins=5;
programs[p14.id]=p14;

TrainingProgram p15; p15.id="IOT101"; p15.name="IoT Systems"; p15.domain="IoT";
p15.scheduleTime="Tue-Thu 4:00 PM"; p15.durationWeeks=10; p15.maxSeats=35;
p15.enrolledSeats=0; p15.feeAmount=1500.0f; p15.totalTopics=18; p15.completedTopics=0;
p15.totalClassesHeld=0; p15.certExamQuestion="What is IoT?";
p15.certExamAnswer="internet of things"; p15.examDurationMins=5;
programs[p15.id]=p15;

TrainingProgram p16; p16.id="RPA101"; p16.name="Robotic Process Automation"; p16.domain="Automation";
p16.scheduleTime="Mon-Fri 1:00 PM"; p16.durationWeeks=7; p16.maxSeats=30;
p16.enrolledSeats=0; p16.feeAmount=1200.0f; p16.totalTopics=14; p16.completedTopics=0;
p16.totalClassesHeld=0; p16.certExamQuestion="What is RPA?";
p16.certExamAnswer="automation using bots"; p16.examDurationMins=5;
programs[p16.id]=p16;

TrainingProgram p17; p17.id="AR101"; p17.name="AR/VR Development"; p17.domain="ARVR";
p17.scheduleTime="Wed-Fri 6:00 PM"; p17.durationWeeks=12; p17.maxSeats=25;
p17.enrolledSeats=0; p17.feeAmount=1700.0f; p17.totalTopics=20; p17.completedTopics=0;
p17.totalClassesHeld=0; p17.certExamQuestion="What is VR?";
p17.certExamAnswer="virtual reality"; p17.examDurationMins=5;
programs[p17.id]=p17;

TrainingProgram p18; p18.id="GAME101"; p18.name="Game Development"; p18.domain="Gaming";
p18.scheduleTime="Weekends 4:00 PM"; p18.durationWeeks=10; p18.maxSeats=30;
p18.enrolledSeats=0; p18.feeAmount=1400.0f; p18.totalTopics=18; p18.completedTopics=0;
p18.totalClassesHeld=0; p18.certExamQuestion="Game engine example?";
p18.certExamAnswer="unity"; p18.examDurationMins=5;
programs[p18.id]=p18;

TrainingProgram p19; p19.id="TEST101"; p19.name="Software Testing"; p19.domain="QA";
p19.scheduleTime="Tue-Thu 3:00 PM"; p19.durationWeeks=6; p19.maxSeats=40;
p19.enrolledSeats=0; p19.feeAmount=900.0f; p19.totalTopics=12; p19.completedTopics=0;
p19.totalClassesHeld=0; p19.certExamQuestion="What is testing?";
p19.certExamAnswer="finding bugs"; p19.examDurationMins=5;
programs[p19.id]=p19;

TrainingProgram p20; p20.id="ENG101"; p20.name="Spoken English"; p20.domain="Communication";
p20.scheduleTime="Daily 8:00 AM"; p20.durationWeeks=5; p20.maxSeats=60;
p20.enrolledSeats=0; p20.feeAmount=500.0f; p20.totalTopics=10; p20.completedTopics=0;
p20.totalClassesHeld=0; p20.certExamQuestion="What is grammar?";
p20.certExamAnswer="rules of language"; p20.examDurationMins=5;
programs[p20.id]=p20;

TrainingProgram p21; p21.id="APT101"; p21.name="Aptitude & Reasoning"; p21.domain="ExamPrep";
p21.scheduleTime="Mon-Fri 9:00 AM"; p21.durationWeeks=6; p21.maxSeats=70;
p21.enrolledSeats=0; p21.feeAmount=600.0f; p21.totalTopics=15; p21.completedTopics=0;
p21.totalClassesHeld=0; p21.certExamQuestion="What is aptitude?";
p21.certExamAnswer="ability to learn"; p21.examDurationMins=5;
programs[p21.id]=p21;

TrainingProgram p22; p22.id="GOV101"; p22.name="Government Exam Prep"; p22.domain="ExamPrep";
p22.scheduleTime="Daily 6:00 AM"; p22.durationWeeks=20; p22.maxSeats=100;
p22.enrolledSeats=0; p22.feeAmount=800.0f; p22.totalTopics=30; p22.completedTopics=0;
p22.totalClassesHeld=0; p22.certExamQuestion="Capital of India?";
p22.certExamAnswer="new delhi"; p22.examDurationMins=5;
programs[p22.id]=p22;

TrainingProgram p23; p23.id="JEE101"; p23.name="JEE Main Preparation"; p23.domain="EntranceExam";
p23.scheduleTime="Mon-Fri 6:00 AM"; p23.durationWeeks=24; p23.maxSeats=80;
p23.enrolledSeats=0; p23.feeAmount=2000.0f; p23.totalTopics=40; p23.completedTopics=0;
p23.totalClassesHeld=0; p23.certExamQuestion="Unit of force?";
p23.certExamAnswer="newton"; p23.examDurationMins=5;
programs[p23.id]=p23;

TrainingProgram p24; p24.id="NEET101"; p24.name="NEET Medical Entrance"; p24.domain="EntranceExam";
p24.scheduleTime="Mon-Sat 5:30 AM"; p24.durationWeeks=24; p24.maxSeats=90;
p24.enrolledSeats=0; p24.feeAmount=2200.0f; p24.totalTopics=45; p24.completedTopics=0;
p24.totalClassesHeld=0; p24.certExamQuestion="Basic unit of life?";
p24.certExamAnswer="cell"; p24.examDurationMins=5;
programs[p24.id]=p24;

TrainingProgram p25; p25.id="GATE101"; p25.name="GATE Engineering Prep"; p25.domain="EntranceExam";
p25.scheduleTime="Weekends 7:00 AM"; p25.durationWeeks=20; p25.maxSeats=70;
p25.enrolledSeats=0; p25.feeAmount=2500.0f; p25.totalTopics=35; p25.completedTopics=0;
p25.totalClassesHeld=0; p25.certExamQuestion="What is CPU?";
p25.certExamAnswer="central processing unit"; p25.examDurationMins=5;
programs[p25.id]=p25;

TrainingProgram p26; p26.id="CAT101"; p26.name="CAT MBA Entrance"; p26.domain="EntranceExam";
p26.scheduleTime="Mon-Fri 8:00 PM"; p26.durationWeeks=16; p26.maxSeats=60;
p26.enrolledSeats=0; p26.feeAmount=1800.0f; p26.totalTopics=30; p26.completedTopics=0;
p26.totalClassesHeld=0; p26.certExamQuestion="What is average?";
p26.certExamAnswer="sum divided by count"; p26.examDurationMins=5;
programs[p26.id]=p26;

TrainingProgram p27; p27.id="UPSC101"; p27.name="UPSC Civil Services Prep"; p27.domain="EntranceExam";
p27.scheduleTime="Daily 7:00 AM"; p27.durationWeeks=52; p27.maxSeats=120;
p27.enrolledSeats=0; p27.feeAmount=3000.0f; p27.totalTopics=60; p27.completedTopics=0;
p27.totalClassesHeld=0; p27.certExamQuestion="Who is President of India?";
p27.certExamAnswer="droupadi murmu"; p27.examDurationMins=5;
programs[p27.id]=p27;

TrainingProgram p28; p28.id="SSC101"; p28.name="SSC Exam Preparation"; p28.domain="EntranceExam";
p28.scheduleTime="Mon-Fri 9:00 AM"; p28.durationWeeks=20; p28.maxSeats=100;
p28.enrolledSeats=0; p28.feeAmount=1200.0f; p28.totalTopics=28; p28.completedTopics=0;
p28.totalClassesHeld=0; p28.certExamQuestion="What is synonym?";
p28.certExamAnswer="same meaning word"; p28.examDurationMins=5;
programs[p28.id]=p28;

TrainingProgram p29; p29.id="BANK101"; p29.name="Banking Exams Prep"; p29.domain="EntranceExam";
p29.scheduleTime="Mon-Fri 10:00 AM"; p29.durationWeeks=18; p29.maxSeats=90;
p29.enrolledSeats=0; p29.feeAmount=1300.0f; p29.totalTopics=25; p29.completedTopics=0;
p29.totalClassesHeld=0; p29.certExamQuestion="What is RBI?";
p29.certExamAnswer="reserve bank of india"; p29.examDurationMins=5;
programs[p29.id]=p29;

TrainingProgram p30; p30.id="TNPSC101"; p30.name="TNPSC Group Exams"; p30.domain="EntranceExam";
p30.scheduleTime="Daily 6:00 PM"; p30.durationWeeks=24; p30.maxSeats=110;
p30.enrolledSeats=0; p30.feeAmount=1000.0f; p30.totalTopics=30; p30.completedTopics=0;
p30.totalClassesHeld=0; p30.certExamQuestion="State capital of Tamil Nadu?";
p30.certExamAnswer="chennai"; p30.examDurationMins=5;
programs[p30.id]=p30;

TrainingProgram p31; p31.id="RRB101"; p31.name="Railway Exams Prep"; p31.domain="EntranceExam";
p31.scheduleTime="Mon-Fri 7:00 PM"; p31.durationWeeks=16; p31.maxSeats=100;
p31.enrolledSeats=0; p31.feeAmount=1100.0f; p31.totalTopics=26; p31.completedTopics=0;
p31.totalClassesHeld=0; p31.certExamQuestion="What is speed?";
p31.certExamAnswer="distance per time"; p31.examDurationMins=5;
programs[p31.id]=p31;

TrainingProgram p32; p32.id="DEF101"; p32.name="Defence Exams (NDA/CDS)"; p32.domain="EntranceExam";
p32.scheduleTime="Morning 5:00 AM"; p32.durationWeeks=20; p32.maxSeats=75;
p32.enrolledSeats=0; p32.feeAmount=1400.0f; p32.totalTopics=28; p32.completedTopics=0;
p32.totalClassesHeld=0; p32.certExamQuestion="Full form of NDA?";
p32.certExamAnswer="national defence academy"; p32.examDurationMins=5;
programs[p32.id]=p32;

hiringPartners.push_back("HCL");
hiringPartners.push_back("IBM");
hiringPartners.push_back("Capgemini");
hiringPartners.push_back("Oracle");
hiringPartners.push_back("Deloitte");
hiringPartners.push_back("EY");
hiringPartners.push_back("KPMG");
hiringPartners.push_back("PwC");
hiringPartners.push_back("Zoho");
hiringPartners.push_back("Tech Mahindra");
hiringPartners.push_back("L&T Infotech");
hiringPartners.push_back("Mindtree");
hiringPartners.push_back("Flipkart");
hiringPartners.push_back("Paytm");
hiringPartners.push_back("Adobe");
hiringPartners.push_back("Salesforce");
hiringPartners.push_back("Intel");
hiringPartners.push_back("Cisco");
hiringPartners.push_back("SAP");
hiringPartners.push_back("Byju's");
hiringPartners.push_back("Freshworks");
hiringPartners.push_back("Swiggy");
hiringPartners.push_back("Zomato");
            saveData();
        }
    }

    ~TrainingInstitute() { saveData(); }

    void run() {
        while (true) {
            if (currentUser.empty()) {
                int ch;
                cout<<"\n=========================================\n"
                    <<"    MODERN TECH TRAINING INSTITUTE\n"
                    <<"=========================================\n"
                    <<"1.Admin  2.Manager  3.Trainer  4.Trainee  5.Front Desk  6.Exit\nChoice: ";
                cin>>ch;
                if(cin.fail()){cin.clear();cin.ignore(1000,'\n');cout<<"Invalid.\n";continue;}
                try {
                    if(ch==1) login("ADMIN"); else if(ch==2) login("MANAGER");
                    else if(ch==3) login("TRAINER"); else if(ch==4) login("TRAINEE");
                    else if(ch==5) frontDeskMenu();
                    else if(ch==6){saveData();break;}
                    else cout<<"Invalid.\n";
                } catch(const SecurityException& e){cout<<e.what()<<"\n";break;}
                continue;
            }
            if(currentRole=="ADMIN")        adminMenu();
            else if(currentRole=="MANAGER") managerMenu();
            else if(currentRole=="TRAINER") trainerMenu();
            else if(currentRole=="TRAINEE") traineeMenu();
            saveData();
            currentUser=""; currentRole="";
        }
    }
};

int main() {
    TrainingInstitute sys;
    sys.run();
    return 0;
}
