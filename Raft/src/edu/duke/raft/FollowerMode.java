package edu.duke.raft;

import java.util.Timer;
import java.util.Random;

public class FollowerMode extends RaftMode {
    protected static Timer timer;
    private boolean leaderIsDead = false;


    // Set a new random variable to be used for our timeout.
    // Set only once to avoid too much object and memory usage
    protected static Random rand = new Random();

    /*
    // Method to detect whether the leader has failed (aka timed out)
    */
    private void setTimeout() {
        long random_timeout = (long) (rand.nextInt((ELECTION_TIMEOUT_MAX - ELECTION_TIMEOUT_MIN) + 1) + ELECTION_TIMEOUT_MIN);
        int override_timeout = mConfig.getTimeoutOverride();
        if (override_timeout != -1) {
            random_timeout = mConfig.getTimeoutOverride();
        }
        timer = scheduleTimer(random_timeout, 0);
    }

    private void resetTimer() {
        timer.cancel();
        setTimeout();
    }


    public void go() {
        synchronized (mLock) {
            int term = mConfig.getCurrentTerm();
            System.out.println("S" +
                    mID +
                    "." +
                    term +
                    ": switched to follower mode.");
            setTimeout();
        }
    }


    private boolean candidateUpToDate(int lastIndex, int lastTerm) {
        if (mLog.getLastTerm() != lastTerm) {
            return mLog.getLastTerm() < lastTerm;
        } else {
            return mLog.getLastIndex() <= lastIndex;
        }

    }

    // @param candidate’s term
    // @param candidate requesting vote
    // @param index of candidate’s last log entry
    // @param term of candidate’s last log entry
    // @return 0, if server votes for candidate; otherwise, server's (0, not true)
    // current term
    //TODO: Implement when doing log replication
    public int requestVote(int candidateTerm,
                           int candidateID,
                           int lastLogIndex,
                           int lastLogTerm) {
        synchronized (mLock) {
            int term = mConfig.getCurrentTerm();
            if (leaderIsDead) {
                return term;
            }
            if (term > candidateTerm) {
                return term; // Return my term so that the candidate updates itself lol
            }

            // 2 possible ways to vote for candidate now.
            // 1). The candidates term is the same as us, and I either haven't voted for them, or I have already voted for them, and are up to date.
            // 2). The candidates term is higher and they are up to date with us

            //1).
            else if (term == candidateTerm) {
                if (candidateUpToDate(lastLogIndex, lastLogTerm) && (mConfig.getVotedFor() == 0 || mConfig.getVotedFor() == candidateID)) {
                    resetTimer();
                    mConfig.setCurrentTerm(candidateTerm, candidateID);
                    return 0; // Return 0, means we voted for the candidate
                } else {
                    return mConfig.getCurrentTerm();
                }
            }

            //2).
            else { //candidateTerm > ourTerm
                if (candidateUpToDate(lastLogIndex, lastLogTerm)) {
                    resetTimer();
                    mConfig.setCurrentTerm(candidateTerm, candidateID);
                    return 0; //Return 0, means we voted for candidate
                } else {
                    // candidateTerm > ourTerm, so we must update our term
                    mConfig.setCurrentTerm(candidateTerm, 0); //
                    return mConfig.getCurrentTerm(); //Return our term
                }


            }
        }
    }


    // @param leader’s term
    // @param current leader
    // @param index of log entry before entries to append
    // @param term of log entry before entries to append
    // @param entries to append (in order of 0 to append.length-1)
    // @param index of highest committed entry
    // @return 0, if server appended entries; otherwise, server's
    // current term
    public int appendEntries(int leaderTerm,
                             int leaderID,
                             int prevLogIndex,
                             int prevLogTerm,
                             Entry[] entries,
                             int leaderCommit) {
        synchronized (mLock) {
            int term = mConfig.getCurrentTerm();
            if (leaderIsDead) {
                return term;
            }
            if (term > leaderTerm) {
                return term;
            }

            //ELSE, a heartbeat message
            // ALSO, Fill out the logging logic

            timer.cancel();
            // If there are no entries, it is a pure heartbeat message, and we can reset the timer
            if (term < leaderTerm) {
                term = leaderTerm;
                mConfig.setCurrentTerm(leaderTerm, 0); //Update object term
            }

            if (entries.length == 0) {
                setTimeout();
                boolean cond1 = prevLogTerm == mLog.getLastTerm();
                boolean cond2 = prevLogIndex == mLog.getLastIndex();
                if (!(cond1) || !(cond2)) {
                    return term;
                } else {
                    return 0;
                }
            }
            // If the term of the heartbeat message is higher than ours, we need to update ourselves

            int index_attempt = mLog.insert(entries, prevLogIndex, prevLogTerm);
            if (index_attempt != -1) {
                setTimeout();
                return 0;
            }


            setTimeout();
            return term; //
        }
    }

    // @param id of the timer that timed out
    public void handleTimeout(int timerID) {
        synchronized (mLock) {

            // HEARTBEAT TIMEROUT, AKA LEADER DID NOT SEND A HEARTBEAT, SO WE WILL BECOME A CANDIDATE
            if (leaderIsDead) {
                return;
            }
            // Right here, there has been a timeout, and the leader hasn't been declared as dead. The only possible explanation is that
            // the leader is now dead. This requires us to set the leader to dead, and start an election. Therefore, we switch to candidate
            // mode
            timer.cancel();
            leaderIsDead = true;
            RaftServerImpl.setMode(new CandidateMode());
            return;
        }
    }
}

